// GeneratorPage: behaviour (payload building, live preview, save / copy).
#include "GeneratorPage.h"

#include "Actions.h"
#include "Theme.h"
#include "Widgets.h"
#include "qrs/Content.h"

#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QImageWriter>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QSpinBox>
#include <QStackedWidget>

namespace app {

void GeneratorPage::setText(const QString &text)
{
    m_type->setCurrentIndex(0);
    m_plainText->setPlainText(text);
    regenerate();
}

void GeneratorPage::applyDefaults()
{
    m_size->setValue(m_settings->defaultSize);
    m_margin->setValue(m_settings->defaultMargin);
    m_ecc->setCurrentIndex(int(m_settings->defaultEcc));
}

void GeneratorPage::scheduleUpdate() { m_debounce.start(); }

// Returns an empty payload with an empty error when the form is simply blank.
QString GeneratorPage::buildPayload(QString *error) const
{
    switch (m_type->currentIndex()) {
    case 0:
        return m_plainText->toPlainText();
    case 1:
        return m_url->text().trimmed().isEmpty() ? QString() : qrs::buildUrl(m_url->text(), error);
    case 2:
        return m_emailTo->text().trimmed().isEmpty()
                   ? QString()
                   : qrs::buildEmail(m_emailTo->text(), m_emailSubject->text(), m_emailBody->toPlainText(), error);
    case 3:
        return m_phone->text().trimmed().isEmpty() ? QString() : qrs::buildPhone(m_phone->text(), error);
    case 4:
        return m_smsNumber->text().trimmed().isEmpty()
                   ? QString()
                   : qrs::buildSms(m_smsNumber->text(), m_smsMessage->toPlainText(), error);
    case 5: {
        if (m_wifiSsid->text().isEmpty())
            return {};
        static const qrs::WifiSecurity map[] = {qrs::WifiSecurity::Wpa, qrs::WifiSecurity::Wep,
                                                qrs::WifiSecurity::Open};
        return qrs::buildWifi(m_wifiSsid->text(), m_wifiPassword->text(),
                              map[qBound(0, m_wifiSecurity->currentIndex(), 2)], m_wifiHidden->isChecked(), error);
    }
    }
    return {};
}

void GeneratorPage::regenerate()
{
    QString error;
    m_payload = buildPayload(&error);

    qrs::GenerateOptions options;
    options.pixelSize = m_size->value();
    options.marginModules = m_margin->value();
    options.ecc = qrs::EccLevel(m_ecc->currentIndex());
    options.foreground = m_fg;
    options.background = m_bg;

    m_result = qrs::GenerateResult();
    if (!m_payload.isEmpty())
        m_result = qrs::generate(m_payload, options);
    else if (error.isEmpty())
        error.clear();
    if (!m_payload.isEmpty() && !m_result.ok())
        error = m_result.error;

    const bool ok = m_result.ok();
    m_saveButton->setEnabled(ok);
    m_copyButton->setEnabled(ok);
    m_copyTextButton->setEnabled(ok);
    m_revealButton->setEnabled(ok);

    const char *role = "muted";
    QString message;
    if (ok) {
        m_preview->setImage(m_result.image);
        m_info->setText(tr("%1 × %1 modules  ·  %2 × %2 px  ·  %3 bytes encoded")
                            .arg(m_result.matrix.size).arg(m_result.image.width()).arg(m_payload.toUtf8().size()));
        if (!m_result.warning.isEmpty()) {
            role = "warning";
            message = m_result.warning;
        } else if (m_result.image.width() != m_size->value()) {
            message = tr("The image was enlarged to %1 px so every module is at least one pixel.")
                          .arg(m_result.image.width());
        }
    } else {
        m_preview->clear();
        m_info->clear();
        if (!error.isEmpty()) {
            role = "error";
            message = error;
            m_preview->setPlaceholder(tr("Cannot create a QR code yet"), tr("Fix the highlighted problem below"));
        } else {
            m_preview->setPlaceholder(tr("Your QR code will appear here"), tr("Start typing on the left"));
        }
    }
    m_message->setProperty("role", role);
    repolish(m_message);
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
}

void GeneratorPage::updateColorButton(QPushButton *button, const QColor &color)
{
    QPixmap swatch(16, 16);
    swatch.fill(color);
    button->setIcon(QIcon(swatch));
    button->setText((button == m_fgButton ? tr("Code %1") : tr("Background %1")).arg(color.name().toUpper()));
}

void GeneratorPage::pickColor(QColor *target, QPushButton *button)
{
    const QColor chosen = QColorDialog::getColor(*target, this,
                                                 button == m_fgButton ? tr("Code colour") : tr("Background colour"));
    if (!chosen.isValid())
        return;
    *target = chosen;
    updateColorButton(button, chosen);
    scheduleUpdate();
}

bool GeneratorPage::saveTo(const QString &path, QString *error)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = file.errorString();
        return false;
    }
    if (suffix == QLatin1String("svg")) {
        qrs::GenerateOptions options;
        options.pixelSize = m_result.image.width();
        options.marginModules = m_margin->value();
        options.foreground = m_fg;
        options.background = m_bg;
        file.write(qrs::renderSvg(m_result.matrix, options));
    } else {
        QImageWriter writer(&file, suffix.isEmpty() ? QByteArray("png") : suffix.toLatin1());
        if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg"))
            writer.setQuality(95);
        if (!writer.write(m_result.image)) {
            *error = writer.errorString();
            file.cancelWriting();
            return false;
        }
    }
    if (!file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}

static QString askSavePath(QWidget *parent, Settings *settings)
{
    QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("Save QR code"), QDir(settings->lastSaveDir).filePath(QStringLiteral("qr-code.png")),
        QObject::tr("PNG image (*.png);;JPEG image (*.jpg);;Bitmap (*.bmp);;WebP image (*.webp);;SVG vector (*.svg)"));
    if (path.isEmpty())
        return {};
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");
    settings->lastSaveDir = QFileInfo(path).absolutePath();
    return path;
}

void GeneratorPage::save()
{
    if (!m_result.ok())
        return;
    const QString path = askSavePath(this, m_settings);
    if (path.isEmpty())
        return;
    QString error;
    if (saveTo(path, &error))
        emit statusMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)));
    else
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save the image:\n%1").arg(error));
}

void GeneratorPage::saveAndReveal()
{
    if (!m_result.ok())
        return;
    const QString path = askSavePath(this, m_settings);
    if (path.isEmpty())
        return;
    QString error;
    if (!saveTo(path, &error)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save the image:\n%1").arg(error));
        return;
    }
    emit statusMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)));
    revealInExplorer(path);
}

void GeneratorPage::copyImage()
{
    if (!m_result.ok())
        return;
    QGuiApplication::clipboard()->setImage(m_result.image);
    emit statusMessage(tr("QR code image copied to clipboard"));
}

} // namespace app
