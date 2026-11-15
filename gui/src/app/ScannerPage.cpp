#include "ScannerPage.h"

#include "Actions.h"
#include "Widgets.h"
#include "qrs/Content.h"
#include "qrs/Files.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSaveFile>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace app {

ScannerPage::ScannerPage(Settings *settings, QWidget *parent) : QWidget(parent), m_settings(settings)
{
    setObjectName("page");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    root->addWidget(makeHeader(tr("QR Scanner"),
                               tr("Open, drop or paste an image to find and decode the QR codes in it.")));

    auto *body = new QHBoxLayout;
    body->setSpacing(16);
    root->addLayout(body, 1);

    // ---- left: image -------------------------------------------------------
    auto *left = new QVBoxLayout;
    left->setSpacing(10);
    auto *toolbar = new QHBoxLayout;
    auto *openButton = makeButton(tr("Open image…"), "primary");
    auto *pasteButton = makeButton(tr("Paste"));
    pasteButton->setToolTip(tr("Scan an image from the clipboard (Ctrl+V)"));
    pasteButton->setShortcut(QKeySequence::Paste);
    m_clearButton = makeButton(tr("Clear"));
    toolbar->addWidget(openButton);
    toolbar->addWidget(pasteButton);
    toolbar->addWidget(m_clearButton);
    toolbar->addStretch();
    left->addLayout(toolbar);

    m_view = new ImageView;
    m_view->setAcceptFileDrops(true);
    m_view->setCheckerboard(true);
    m_view->setPlaceholder(tr("Drop an image here"), tr("or click to browse  ·  PNG, JPG, BMP, GIF, WebP, TIFF"));
    left->addWidget(m_view, 1);
    m_fileLabel = makeLabel(QString(), "muted");
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    left->addWidget(m_fileLabel);
    body->addLayout(left, 3);

    // ---- right: result -----------------------------------------------------
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    frame->setMinimumWidth(340);
    frame->setMaximumWidth(460);
    card->addWidget(makeLabel(tr("RESULT"), "section"));
    m_statusLabel = makeLabel(tr("No image scanned yet"));
    m_statusLabel->setWordWrap(true);
    QFont big = m_statusLabel->font();
    big.setPointSizeF(big.pointSizeF() + 3);
    big.setWeight(QFont::DemiBold);
    m_statusLabel->setFont(big);
    card->addWidget(m_statusLabel);
    m_hintLabel = makeLabel(tr("Decoded content will appear here."), "muted");
    m_hintLabel->setWordWrap(true);
    card->addWidget(m_hintLabel);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    card->addWidget(m_progress);

    m_codeList = new QListWidget;
    m_codeList->setMaximumHeight(110);
    m_codeList->setVisible(false);
    card->addWidget(m_codeList);

    m_kindLabel = makeLabel(QString(), "section");
    card->addWidget(m_kindLabel);
    m_text = new QPlainTextEdit;
    m_text->setReadOnly(true);
    m_text->setPlaceholderText(tr("Nothing decoded"));
    card->addWidget(m_text, 1);
    m_warningLabel = makeLabel(QString(), "warning");
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setVisible(false);
    card->addWidget(m_warningLabel);
    m_detailLabel = makeLabel(QString(), "muted");
    m_detailLabel->setWordWrap(true);
    card->addWidget(m_detailLabel);

    auto *actions = new QHBoxLayout;
    m_copyButton = makeButton(tr("Copy"), "primary");
    m_openButton = makeButton(tr("Open link"));
    m_exportButton = makeButton(tr("Export…"));
    actions->addWidget(m_copyButton);
    actions->addWidget(m_openButton);
    actions->addWidget(m_exportButton);
    card->addLayout(actions);
    body->addWidget(frame, 2);

    connect(openButton, &QPushButton::clicked, this, &ScannerPage::chooseFile);
    connect(pasteButton, &QPushButton::clicked, this, &ScannerPage::pasteImage);
    connect(m_clearButton, &QPushButton::clicked, this, &ScannerPage::clearAll);
    connect(m_view, &ImageView::fileDropped, this, &ScannerPage::scanFile);
    connect(m_view, &ImageView::clicked, this, [this] {
        if (m_view->image().isNull())
            chooseFile();
    });
    connect(m_codeList, &QListWidget::currentRowChanged, this, &ScannerPage::showCode);
    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        if (const qrs::QrCode *code = currentCode()) {
            copyText(code->text);
            emit statusMessage(tr("Copied to clipboard"));
        }
    });
    connect(m_openButton, &QPushButton::clicked, this, [this] {
        if (const qrs::QrCode *code = currentCode())
            openLink(this, qrs::classify(code->text), m_settings->confirmOpenLinks);
    });
    connect(m_exportButton, &QPushButton::clicked, this, &ScannerPage::exportResults);
    connect(&m_watcher, &QFutureWatcher<ScanJob>::finished, this, &ScannerPage::onScanFinished);

    showCode(-1);
}

void ScannerPage::chooseFile()
{
    QStringList patterns;
    for (const QString &ext : qrs::supportedImageExtensions())
        patterns << QStringLiteral("*.") + ext;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open image"), m_settings->lastOpenDir,
        tr("Images (%1);;All files (*)").arg(patterns.join(u' ')));
    if (path.isEmpty())
        return;
    m_settings->lastOpenDir = QFileInfo(path).absolutePath();
    scanFile(path);
}

void ScannerPage::pasteImage()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (mime && mime->hasUrls() && !mime->urls().isEmpty() && mime->urls().first().isLocalFile()) {
        scanFile(mime->urls().first().toLocalFile());
        return;
    }
    const QImage image = QApplication::clipboard()->image();
    if (image.isNull()) {
        emit statusMessage(tr("The clipboard does not contain an image"));
        return;
    }
    startScan(image, QString());
}

void ScannerPage::scanFile(const QString &path) { startScan(QImage(), path); }

void ScannerPage::startScan(const QImage &image, const QString &path)
{
    if (m_watcher.isRunning()) {
        // Only the most recent request matters; run it when the current one ends.
        m_pending = true;
        m_pendingImage = image;
        m_pendingPath = path;
        return;
    }
    const int generation = ++m_generation;
    setBusy(true);
    m_fileLabel->setText(path.isEmpty() ? tr("Image from clipboard") : QDir::toNativeSeparators(path));
    m_watcher.setFuture(QtConcurrent::run([image, path, generation] {
        ScanJob job;
        job.generation = generation;
        if (path.isEmpty()) {
            job.image = image;
            job.result = qrs::scanImage(image);
        } else {
            job.result = qrs::scanFile(path, &job.image);
        }
        return job;
    }));
}

void ScannerPage::onScanFinished()
{
    const ScanJob job = m_watcher.result();
    if (m_pending) {
        m_pending = false;
        const QImage image = m_pendingImage;
        m_pendingImage = QImage();
        startScan(image, m_pendingPath);
        return;
    }
    setBusy(false);
    if (job.generation != m_generation)
        return;
    m_view->setImage(job.image);
    showResult(job.result);
}

void ScannerPage::setBusy(bool busy)
{
    m_progress->setVisible(busy);
    if (busy) {
        m_statusLabel->setProperty("role", QVariant());
        repolish(m_statusLabel);
        m_statusLabel->setText(tr("Scanning…"));
        m_hintLabel->setText(tr("Trying several detection passes."));
        m_hintLabel->setVisible(true);
    }
}

void ScannerPage::showResult(const qrs::ScanResult &result)
{
    m_result = result;
    m_decodedIndex.clear();
    m_codeList->blockSignals(true);
    m_codeList->clear();

    QVector<ImageView::Outline> outlines;
    for (int i = 0; i < result.codes.size(); ++i) {
        const qrs::QrCode &code = result.codes[i];
        outlines.append({code.corners, code.decoded});
        if (code.decoded) {
            m_decodedIndex << i;
            QString preview = code.text.simplified();
            if (preview.size() > 60)
                preview = preview.left(60) + QStringLiteral("…");
            m_codeList->addItem(QStringLiteral("%1.  %2").arg(m_decodedIndex.size()).arg(preview));
        }
    }
    m_codeList->blockSignals(false);
    m_view->setOutlines(outlines);
    m_codeList->setVisible(m_decodedIndex.size() > 1);

    const char *role = nullptr;
    switch (result.status) {
    case qrs::ScanStatus::Found:
        role = "success";
        m_statusLabel->setText(m_decodedIndex.size() == 1 ? tr("QR code found")
                                                          : tr("%1 QR codes found").arg(m_decodedIndex.size()));
        m_hintLabel->setVisible(false);
        emit statusMessage(tr("Decoded %1 code(s) in %2 ms").arg(m_decodedIndex.size()).arg(result.elapsedMs));
        break;
    case qrs::ScanStatus::NoQr:
        m_statusLabel->setText(tr("No QR found"));
        m_hintLabel->setText(
            result.codes.isEmpty()
                ? tr("No QR code could be detected in this image. Try another image, or a sharper, "
                     "closer or better-lit picture of the code.")
                : tr("Something that looks like a QR code was located (highlighted), but it is too "
                     "damaged or blurry to read (%1). Try a sharper image.").arg(result.codes.first().failure));
        m_hintLabel->setVisible(true);
        emit statusMessage(tr("No QR found (%1 ms, passes: %2)")
                               .arg(result.elapsedMs).arg(result.strategiesTried.join(QStringLiteral(", "))));
        break;
    case qrs::ScanStatus::LoadError:
        role = "error";
        m_statusLabel->setText(tr("Could not read the image"));
        m_hintLabel->setText(result.error);
        m_hintLabel->setVisible(true);
        emit statusMessage(result.error);
        break;
    }
    m_statusLabel->setProperty("role", role ? QVariant(role) : QVariant());
    repolish(m_statusLabel);

    if (m_decodedIndex.isEmpty())
        showCode(-1);
    else
        m_codeList->setCurrentRow(0), showCode(0);
}

const qrs::QrCode *ScannerPage::currentCode() const
{
    int row = m_codeList->currentRow();
    if (row < 0 && m_decodedIndex.size() == 1)
        row = 0;
    if (row < 0 || row >= m_decodedIndex.size())
        return nullptr;
    return &m_result.codes[m_decodedIndex[row]];
}

void ScannerPage::showCode(int row)
{
    const bool valid = row >= 0 && row < m_decodedIndex.size();
    m_copyButton->setEnabled(valid);
    m_exportButton->setEnabled(valid);
    m_openButton->setEnabled(false);
    m_warningLabel->setVisible(false);
    if (!valid) {
        m_text->clear();
        m_kindLabel->clear();
        m_detailLabel->clear();
        return;
    }
    const qrs::QrCode &code = m_result.codes[m_decodedIndex[row]];
    const qrs::ContentInfo info = qrs::classify(code.text);
    m_text->setPlainText(code.text);
    m_kindLabel->setText(info.label.toUpper());
    m_openButton->setEnabled(info.canOpen());
    m_openButton->setToolTip(info.canOpen() ? tr("Asks for confirmation before opening")
                                            : tr("This content is not a link that can be opened"));
    if (!info.warning.isEmpty()) {
        m_warningLabel->setText(info.warning);
        m_warningLabel->setVisible(true);
    }
    static const char *const ecc[] = {"L", "M", "Q", "H"};
    m_detailLabel->setText(tr("Version %1  ·  ECC %2  ·  %3 bytes  ·  pass: %4%5")
                               .arg(code.version).arg(QLatin1String(ecc[qBound(0, code.eccLevel, 3)]))
                               .arg(code.raw.size()).arg(code.strategy,
                                    code.mirrored ? tr("  ·  mirrored") : QString()));
}

void ScannerPage::exportResults()
{
    if (m_decodedIndex.isEmpty())
        return;
    QString filter;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export decoded data"), QDir(m_settings->lastSaveDir).filePath(QStringLiteral("qr-result.txt")),
        tr("Text file (*.txt);;CSV file (*.csv)"), &filter);
    if (path.isEmpty())
        return;
    m_settings->lastSaveDir = QFileInfo(path).absolutePath();

    QString error;
    bool ok;
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        ok = qrs::writeCsv(path, {m_result}, &error);
    } else {
        QSaveFile file(path);
        ok = file.open(QIODevice::WriteOnly);
        if (ok) {
            file.write(m_result.texts().join(QStringLiteral("\r\n")).toUtf8());
            ok = file.commit();
        }
        error = file.errorString();
    }
    if (ok)
        emit statusMessage(tr("Exported to %1").arg(QDir::toNativeSeparators(path)));
    else
        QMessageBox::warning(this, tr("Export failed"), tr("Could not write the file:\n%1").arg(error));
}

void ScannerPage::clearAll()
{
    ++m_generation; // drop any scan still in flight
    m_pending = false;
    m_result = qrs::ScanResult();
    m_decodedIndex.clear();
    m_codeList->clear();
    m_codeList->setVisible(false);
    m_view->clear();
    m_fileLabel->clear();
    m_statusLabel->setProperty("role", QVariant());
    repolish(m_statusLabel);
    m_statusLabel->setText(tr("No image scanned yet"));
    m_hintLabel->setText(tr("Decoded content will appear here."));
    m_hintLabel->setVisible(true);
    showCode(-1);
}

} // namespace app
