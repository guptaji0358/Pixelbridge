#include "BatchPage.h"

#include "Actions.h"
#include "Theme.h"
#include "Widgets.h"
#include "qrs/Content.h"
#include "qrs/Files.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace app {

enum { ColFile, ColStatus, ColContent, ColType, ColFolder, ColCount };

BatchPage::BatchPage(Settings *settings, QWidget *parent) : QWidget(parent), m_settings(settings)
{
    setObjectName("page");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);
    root->addWidget(makeHeader(tr("Batch Processing"),
                               tr("Scan every supported image in a folder and export the results to CSV.")));

    auto *row = new QHBoxLayout;
    m_folderEdit = new QLineEdit;
    m_folderEdit->setPlaceholderText(tr("Folder to scan"));
    auto *browse = makeButton(tr("Browse…"));
    m_recursive = new QCheckBox(tr("Include subfolders"));
    m_startButton = makeButton(tr("Scan folder"), "primary");
    m_cancelButton = makeButton(tr("Cancel"));
    row->addWidget(m_folderEdit, 1);
    row->addWidget(browse);
    row->addWidget(m_recursive);
    row->addWidget(m_startButton);
    row->addWidget(m_cancelButton);
    root->addLayout(row);

    auto *progressRow = new QHBoxLayout;
    m_progress = new QProgressBar;
    m_progressLabel = makeLabel(QString(), "muted");
    progressRow->addWidget(m_progress, 1);
    progressRow->addWidget(m_progressLabel);
    root->addLayout(progressRow);

    m_table = new QTableWidget(0, ColCount);
    m_table->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Content"), tr("Type"), tr("Folder")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColContent, QHeaderView::Stretch);
    m_table->setColumnWidth(ColFile, 220);
    m_table->setColumnWidth(ColStatus, 100);
    m_table->setColumnWidth(ColType, 120);
    m_table->setColumnWidth(ColFolder, 200);
    root->addWidget(m_table, 1);

    m_emptyLabel = makeLabel(tr("No batch has been run yet.\nChoose a folder and press “Scan folder”, "
                                "or select files in the File Explorer and choose “Batch scan selected”."),
                             "muted");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    root->addWidget(m_emptyLabel, 1);

    auto *bottom = new QHBoxLayout;
    m_summary = makeLabel(QString(), "muted");
    m_copyButton = makeButton(tr("Copy all results"));
    m_exportButton = makeButton(tr("Export CSV…"), "primary");
    bottom->addWidget(m_summary, 1);
    bottom->addWidget(m_copyButton);
    bottom->addWidget(m_exportButton);
    root->addLayout(bottom);

    m_folderEdit->setText(QDir::toNativeSeparators(m_settings->lastBrowseDir));
    connect(browse, &QPushButton::clicked, this, &BatchPage::chooseFolder);
    connect(m_startButton, &QPushButton::clicked, this, &BatchPage::startFolder);
    connect(m_folderEdit, &QLineEdit::returnPressed, this, &BatchPage::startFolder);
    connect(m_cancelButton, &QPushButton::clicked, this, &BatchPage::cancel);
    connect(m_exportButton, &QPushButton::clicked, this, &BatchPage::exportCsv);
    connect(m_copyButton, &QPushButton::clicked, this, &BatchPage::copyAll);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, &BatchPage::showContextMenu);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        emit scanRequested(m_table->item(r, ColFile)->data(Qt::UserRole).toString());
    });
    connect(&m_watcher, &QFutureWatcher<qrs::ScanResult>::resultReadyAt, this, &BatchPage::onResult);
    connect(&m_watcher, &QFutureWatcher<qrs::ScanResult>::finished, this, &BatchPage::onFinished);

    setRunning(false);
    updateSummary();
}

void BatchPage::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a folder to scan"),
                                                          m_folderEdit->text());
    if (!dir.isEmpty())
        m_folderEdit->setText(QDir::toNativeSeparators(dir));
}

void BatchPage::startFolder()
{
    if (isRunning())
        return;
    const QString folder = QDir::fromNativeSeparators(m_folderEdit->text().trimmed());
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        QMessageBox::information(this, tr("Batch scan"), tr("Choose an existing folder first."));
        return;
    }
    qrs::SearchQuery query;
    query.recursive = m_recursive->isChecked();
    QStringList paths;
    for (const QFileInfo &f : qrs::findFiles(folder, query))
        paths << f.absoluteFilePath();
    if (paths.isEmpty()) {
        QMessageBox::information(this, tr("Batch scan"), tr("There are no supported images in this folder."));
        return;
    }
    start(paths);
}

void BatchPage::scanFiles(const QStringList &paths)
{
    if (isRunning()) {
        QMessageBox::information(this, tr("Batch scan"), tr("A batch is already running. Cancel it first."));
        return;
    }
    if (!paths.isEmpty())
        start(paths);
}

void BatchPage::start(const QStringList &paths)
{
    m_results.clear();
    m_table->setRowCount(0);
    m_total = int(paths.size());
    m_found = m_none = m_errors = m_codes = 0;
    m_progress->setRange(0, m_total);
    m_progress->setValue(0);
    setRunning(true);
    updateSummary();
    // Leave one core free so the interface stays responsive.
    m_watcher.setFuture(QtConcurrent::mapped(paths, [](const QString &path) { return qrs::scanFile(path); }));
}

void BatchPage::cancel()
{
    if (isRunning()) {
        m_watcher.cancel();
        m_progressLabel->setText(tr("Cancelling…"));
    }
}

void BatchPage::onResult(int index)
{
    const qrs::ScanResult r = m_watcher.resultAt(index);
    m_results << r;
    const ThemeColors &c = currentTheme();
    const QFileInfo info(r.filePath);

    QString status, content, type;
    QColor color = c.textMuted;
    switch (r.status) {
    case qrs::ScanStatus::Found: {
        ++m_found;
        const QStringList texts = r.texts();
        m_codes += int(texts.size());
        status = texts.size() == 1 ? tr("QR found") : tr("%1 QR codes").arg(texts.size());
        content = texts.join(QStringLiteral("  |  ")).simplified();
        type = qrs::classify(texts.first()).label;
        color = c.success;
        break;
    }
    case qrs::ScanStatus::NoQr:
        ++m_none;
        status = tr("No QR found");
        break;
    case qrs::ScanStatus::LoadError:
        ++m_errors;
        status = tr("Error");
        content = r.error;
        color = c.danger;
        break;
    }

    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto *fileItem = new QTableWidgetItem(info.fileName());
    fileItem->setData(Qt::UserRole, r.filePath);
    fileItem->setToolTip(QDir::toNativeSeparators(r.filePath));
    auto *statusItem = new QTableWidgetItem(status);
    statusItem->setForeground(color);
    auto *contentItem = new QTableWidgetItem(content);
    contentItem->setToolTip(content.left(1000));
    m_table->setItem(row, ColFile, fileItem);
    m_table->setItem(row, ColStatus, statusItem);
    m_table->setItem(row, ColContent, contentItem);
    m_table->setItem(row, ColType, new QTableWidgetItem(type));
    m_table->setItem(row, ColFolder, new QTableWidgetItem(QDir::toNativeSeparators(info.absolutePath())));

    m_progress->setValue(int(m_results.size()));
    updateSummary();
}

void BatchPage::onFinished()
{
    const bool cancelled = m_watcher.isCanceled();
    setRunning(false);
    updateSummary();
    emit statusMessage(cancelled ? tr("Batch cancelled after %1 of %2 files").arg(m_results.size()).arg(m_total)
                                 : tr("Batch finished: %1 of %2 files contain a QR code").arg(m_found).arg(m_total));
}

void BatchPage::setRunning(bool running)
{
    m_startButton->setEnabled(!running);
    m_folderEdit->setEnabled(!running);
    m_recursive->setEnabled(!running);
    m_cancelButton->setEnabled(running);
    m_progress->setVisible(running);
    m_progressLabel->setVisible(running);
}

void BatchPage::updateSummary()
{
    const bool any = !m_results.isEmpty();
    const bool running = isRunning();
    m_table->setVisible(any || running);
    m_emptyLabel->setVisible(!any && !running);
    m_exportButton->setEnabled(any && !running);
    m_copyButton->setEnabled(m_found > 0 && !running);
    m_progressLabel->setText(tr("%1 of %2").arg(m_results.size()).arg(m_total));
    m_summary->setText(any ? tr("%1 scanned  ·  %2 with QR (%3 codes)  ·  %4 without  ·  %5 errors")
                                 .arg(m_results.size()).arg(m_found).arg(m_codes).arg(m_none).arg(m_errors)
                           : QString());
}

void BatchPage::exportCsv()
{
    if (m_results.isEmpty())
        return;
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export results"), QDir(m_settings->lastSaveDir).filePath(QStringLiteral("qr-scan-results.csv")),
        tr("CSV file (*.csv)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".csv");
    m_settings->lastSaveDir = QFileInfo(path).absolutePath();
    QString error;
    if (qrs::writeCsv(path, m_results, &error))
        emit statusMessage(tr("Exported %1 rows to %2").arg(m_results.size()).arg(QDir::toNativeSeparators(path)));
    else
        QMessageBox::warning(this, tr("Export failed"), tr("Could not write the CSV file:\n%1").arg(error));
}

void BatchPage::copyAll()
{
    QStringList lines;
    for (const qrs::ScanResult &r : m_results)
        lines << r.texts();
    if (lines.isEmpty())
        return;
    copyText(lines.join(QStringLiteral("\r\n")));
    emit statusMessage(tr("%1 decoded value(s) copied to clipboard").arg(lines.size()));
}

void BatchPage::showContextMenu(const QPoint &pos)
{
    const int row = m_table->rowAt(pos.y());
    if (row < 0)
        return;
    const QString path = m_table->item(row, ColFile)->data(Qt::UserRole).toString();
    const QString content = m_table->item(row, ColContent)->text();
    QMenu menu(this);
    menu.addAction(tr("Open with QR Scanner"), this, [this, path] { emit scanRequested(path); });
    menu.addAction(tr("Open containing folder"), this, [path] { revealInExplorer(path); });
    menu.addAction(tr("Copy file path"), this, [path] { copyText(QDir::toNativeSeparators(path)); });
    QAction *copy = menu.addAction(tr("Copy content"), this, [content] { copyText(content); });
    copy->setEnabled(!content.isEmpty());
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

} // namespace app
