#include "SettingsPage.h"

#include "Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#ifndef QRS_TOOLS_DIR
#define QRS_TOOLS_DIR ""
#endif

namespace app {

SettingsPage::SettingsPage(Settings *settings, QWidget *parent) : QWidget(parent), m_settings(settings)
{
    setObjectName("page");
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 20, 12, 20);
    outer->setSpacing(14);
    outer->addWidget(makeHeader(tr("Settings"), tr("Preferences are saved automatically.")));

    auto *host = new QWidget;
    host->setObjectName("page");
    auto *column = new QVBoxLayout(host);
    column->setContentsMargins(0, 0, 12, 0);
    column->setSpacing(14);
    column->addWidget(buildAppearance());
    column->addWidget(buildGenerator());
    column->addWidget(buildIntegration());
    auto *about = makeLabel(tr("QR Studio %1  ·  works fully offline  ·  settings file: %2")
                                .arg(QCoreApplication::applicationVersion(),
                                     QDir::toNativeSeparators(Settings::filePath())),
                            "muted");
    about->setWordWrap(true);
    about->setTextInteractionFlags(Qt::TextSelectableByMouse);
    column->addWidget(about);
    column->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(host);
    outer->addWidget(scroll, 1);
}

QWidget *SettingsPage::buildAppearance()
{
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    card->addWidget(makeLabel(tr("APPEARANCE AND SAFETY"), "section"));
    auto *form = new QFormLayout;
    form->setSpacing(8);
    m_theme = new QComboBox;
    m_theme->addItems({tr("Follow Windows"), tr("Light"), tr("Dark")});
    m_theme->setCurrentIndex(int(m_settings->theme));
    m_theme->setMaximumWidth(240);
    form->addRow(tr("Theme"), m_theme);
    m_confirmLinks = new QCheckBox(tr("Ask before opening links found in QR codes (recommended)"));
    m_confirmLinks->setChecked(m_settings->confirmOpenLinks);
    form->addRow(tr("Links"), m_confirmLinks);
    card->addLayout(form);

    connect(m_theme, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_settings->theme = ThemeMode(index);
        m_settings->save();
        emit themeChanged();
    });
    connect(m_confirmLinks, &QCheckBox::toggled, this, [this](bool on) {
        m_settings->confirmOpenLinks = on;
        m_settings->save();
    });
    return frame;
}

QWidget *SettingsPage::buildGenerator()
{
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    card->addWidget(makeLabel(tr("GENERATOR DEFAULTS"), "section"));
    auto *form = new QFormLayout;
    form->setSpacing(8);
    m_size = new QSpinBox;
    m_size->setRange(qrs::limits::MinPixelSize, qrs::limits::MaxPixelSize);
    m_size->setSingleStep(32);
    m_size->setSuffix(tr(" px"));
    m_size->setValue(m_settings->defaultSize);
    m_margin = new QSpinBox;
    m_margin->setRange(0, qrs::limits::MaxMargin);
    m_margin->setSuffix(tr(" modules"));
    m_margin->setValue(m_settings->defaultMargin);
    m_ecc = new QComboBox;
    m_ecc->addItems({tr("Low (7%)"), tr("Medium (15%)"), tr("Quartile (25%)"), tr("High (30%)")});
    m_ecc->setCurrentIndex(int(m_settings->defaultEcc));
    for (QWidget *w : {static_cast<QWidget *>(m_size), static_cast<QWidget *>(m_margin),
                       static_cast<QWidget *>(m_ecc)})
        w->setMaximumWidth(240);
    form->addRow(tr("Image size"), m_size);
    form->addRow(tr("Margin"), m_margin);
    form->addRow(tr("Error correction"), m_ecc);
    card->addLayout(form);

    auto store = [this] {
        m_settings->defaultSize = m_size->value();
        m_settings->defaultMargin = m_margin->value();
        m_settings->defaultEcc = qrs::EccLevel(m_ecc->currentIndex());
        m_settings->save();
        emit generatorDefaultsChanged();
    };
    connect(m_size, &QSpinBox::editingFinished, this, store);
    connect(m_margin, &QSpinBox::editingFinished, this, store);
    connect(m_ecc, &QComboBox::currentIndexChanged, this, store);
    return frame;
}

QWidget *SettingsPage::buildIntegration()
{
    QVBoxLayout *card = nullptr;
    QFrame *frame = makeCard(&card);
    card->addWidget(makeLabel(tr("FILE EXPLORER RIGHT-CLICK MENU"), "section"));
    auto *text = makeLabel(
        tr("Adds a “QR Studio” submenu to the right-click menu of image files (Open with QR Scanner, "
           "Copy file path, Open with default application) and of .txt files (Generate QR from text).\n"
           "It is added for your user account only, next to the existing entries; default programs and "
           "other file types are not changed. On Windows 11 it appears under “Show more options”."));
    text->setWordWrap(true);
    card->addWidget(text);
    m_integrationStatus = makeLabel(tr("Status: checking…"), "muted");
    m_integrationStatus->setWordWrap(true);
    card->addWidget(m_integrationStatus);
    auto *row = new QHBoxLayout;
    m_installButton = makeButton(tr("Add to right-click menu…"), "primary");
    m_removeButton = makeButton(tr("Remove…"));
    row->addWidget(m_installButton);
    row->addWidget(m_removeButton);
    row->addStretch();
    card->addLayout(row);
    connect(m_installButton, &QPushButton::clicked, this, [this] { changeIntegration(true); });
    connect(m_removeButton, &QPushButton::clicked, this, [this] { changeIntegration(false); });
    return frame;
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshIntegrationStatus();
}

QString SettingsPage::scriptPath()
{
    const QString name = QStringLiteral("context_menu.ps1");
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(name),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tools/windows/") + name),
        QDir(QStringLiteral(QRS_TOOLS_DIR)).filePath(QStringLiteral("windows/") + name)};
    for (const QString &c : candidates)
        if (QFileInfo(c).isFile())
            return c;
    return {};
}

bool SettingsPage::runScript(const QStringList &arguments, QString *output)
{
    const QString script = scriptPath();
    if (script.isEmpty()) {
        *output = tr("context_menu.ps1 was not found next to the application.");
        return false;
    }
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    QStringList args = {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                        QStringLiteral("-File"), QDir::toNativeSeparators(script)};
    args += arguments;
    process.start(QStringLiteral("powershell.exe"), args);
    if (!process.waitForFinished(20000)) {
        process.kill();
        *output = tr("PowerShell did not respond.");
        return false;
    }
    *output = QString::fromLocal8Bit(process.readAll()).trimmed();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void SettingsPage::refreshIntegrationStatus()
{
    QString output;
    const bool ok = runScript({QStringLiteral("-Action"), QStringLiteral("status")}, &output);
    const bool installed = ok && output.contains(QLatin1String("status: installed"));
    if (!ok)
        m_integrationStatus->setText(tr("Status: unknown (%1)").arg(output));
    else if (installed)
        m_integrationStatus->setText(tr("Status: installed\n%1").arg(output.section(u'\n', 1)));
    else
        m_integrationStatus->setText(tr("Status: not installed"));
    m_installButton->setEnabled(ok);
    m_installButton->setText(installed ? tr("Repair / update…") : tr("Add to right-click menu…"));
    m_removeButton->setEnabled(installed);
}

void SettingsPage::changeIntegration(bool install)
{
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(install ? tr("Add to right-click menu") : tr("Remove from right-click menu"));
    box.setText(install ? tr("Add QR Studio to the File Explorer right-click menu?")
                        : tr("Remove QR Studio from the File Explorer right-click menu?"));
    box.setInformativeText(
        install ? tr("This writes registry keys for your user account only:\n\n"
                     "HKEY_CURRENT_USER\\Software\\Classes\\SystemFileAssociations\\<type>\\shell\\QRStudio\n\n"
                     "for .png .jpg .jpeg .bmp .gif .webp .tif .tiff and .txt, pointing to:\n%1\n\n"
                     "No administrator rights are used. If you move this program later, run this again. "
                     "You can remove the entries here at any time.").arg(exe)
                : tr("This deletes only the “QRStudio” keys under\n"
                     "HKEY_CURRENT_USER\\Software\\Classes\\SystemFileAssociations.\nNothing else is changed."));
    QPushButton *confirm = box.addButton(install ? tr("Add") : tr("Remove"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != confirm)
        return;

    QStringList args = {QStringLiteral("-Action"), install ? QStringLiteral("install") : QStringLiteral("uninstall")};
    if (install)
        args << QStringLiteral("-ExePath") << exe;
    QString output;
    if (runScript(args, &output))
        emit statusMessage(install ? tr("Right-click menu entries added") : tr("Right-click menu entries removed"));
    else
        QMessageBox::warning(this, box.windowTitle(), tr("The change could not be completed:\n%1").arg(output));
    refreshIntegrationStatus();
}

} // namespace app
