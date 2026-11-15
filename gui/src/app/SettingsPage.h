#pragma once

#include "AppSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace app {

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(Settings *settings, QWidget *parent = nullptr);

signals:
    void themeChanged();
    void generatorDefaultsChanged();
    void statusMessage(const QString &message);

protected:
    void showEvent(QShowEvent *) override;

private:
    QWidget *buildAppearance();
    QWidget *buildGenerator();
    QWidget *buildIntegration();
    void refreshIntegrationStatus();
    void changeIntegration(bool install);

    // Runs tools/windows/context_menu.ps1; returns its combined output.
    static QString scriptPath();
    static bool runScript(const QStringList &arguments, QString *output);

    Settings *m_settings;
    QComboBox *m_theme;
    QSpinBox *m_size, *m_margin;
    QComboBox *m_ecc;
    QCheckBox *m_confirmLinks;
    QLabel *m_integrationStatus;
    QPushButton *m_installButton, *m_removeButton;
};

} // namespace app
