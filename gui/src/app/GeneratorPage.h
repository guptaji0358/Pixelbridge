#pragma once

#include "AppSettings.h"
#include "qrs/Generator.h"

#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace app {

class ImageView;

class GeneratorPage : public QWidget {
    Q_OBJECT
public:
    explicit GeneratorPage(Settings *settings, QWidget *parent = nullptr);

    // Pre-fills the plain-text form (used by the command line / other pages).
    void setText(const QString &text);
    // Re-reads size / margin / ECC defaults after Settings changed.
    void applyDefaults();

signals:
    void statusMessage(const QString &message);

private:
    QWidget *buildForms();
    QWidget *buildOptions();
    QString buildPayload(QString *error) const;
    void scheduleUpdate();
    void regenerate();
    void pickColor(QColor *target, QPushButton *button);
    void updateColorButton(QPushButton *button, const QColor &color);
    void save();
    void copyImage();
    void saveAndReveal();
    bool saveTo(const QString &path, QString *error);

    Settings *m_settings;
    QTimer m_debounce;

    QComboBox *m_type;
    QStackedWidget *m_forms;
    QPlainTextEdit *m_plainText;
    QLineEdit *m_url;
    QLineEdit *m_emailTo, *m_emailSubject;
    QPlainTextEdit *m_emailBody;
    QLineEdit *m_phone;
    QLineEdit *m_smsNumber;
    QPlainTextEdit *m_smsMessage;
    QLineEdit *m_wifiSsid, *m_wifiPassword;
    QComboBox *m_wifiSecurity;
    QCheckBox *m_wifiHidden;

    QSpinBox *m_size, *m_margin;
    QComboBox *m_ecc;
    QPushButton *m_fgButton, *m_bgButton;
    QColor m_fg = Qt::black, m_bg = Qt::white;

    ImageView *m_preview;
    QLabel *m_message;
    QLabel *m_info;
    QPushButton *m_saveButton, *m_copyButton, *m_copyTextButton, *m_revealButton;

    qrs::GenerateResult m_result;
    QString m_payload;
};

} // namespace app
