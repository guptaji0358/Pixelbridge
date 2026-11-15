#include "MainWindow.h"

#include "BatchPage.h"
#include "FilesPage.h"
#include "GeneratorPage.h"
#include "ScannerPage.h"
#include "SettingsPage.h"
#include "Theme.h"
#include "Widgets.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

namespace app {

QIcon MainWindow::appIcon()
{
    // Drawn in code so the build needs no binary assets: a rounded tile with
    // three finder-like squares.
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#2f6fed"));
        p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);
        const qreal u = size / 16.0;
        auto finder = [&](qreal x, qreal y) {
            p.setBrush(Qt::white);
            p.drawRect(QRectF(x * u, y * u, 5 * u, 5 * u));
            p.setBrush(QColor("#2f6fed"));
            p.drawRect(QRectF((x + 1) * u, (y + 1) * u, 3 * u, 3 * u));
            p.setBrush(Qt::white);
            p.drawRect(QRectF((x + 2) * u, (y + 2) * u, u, u));
        };
        finder(2, 2);
        finder(9, 2);
        finder(2, 9);
        p.setBrush(Qt::white);
        p.drawRect(QRectF(9 * u, 9 * u, 2 * u, 2 * u));
        p.drawRect(QRectF(12 * u, 12 * u, 2 * u, 2 * u));
        p.drawRect(QRectF(12 * u, 9 * u, 2 * u, u));
        p.drawRect(QRectF(9 * u, 12 * u, u, 2 * u));
        icon.addPixmap(pm);
    }
    return icon;
}

MainWindow::MainWindow(Settings *settings, QWidget *parent) : QMainWindow(parent), m_settings(settings)
{
    setWindowTitle(tr("QR Studio"));
    setWindowIcon(appIcon());
    resize(1180, 760);
    setMinimumSize(920, 600);

    auto *central = new QWidget;
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(210);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(12, 16, 12, 12);
    side->setSpacing(4);
    auto *brand = new QLabel(tr("QR Studio"));
    brand->setObjectName("brand");
    side->addWidget(brand);
    m_nav = new QListWidget;
    m_nav->setObjectName("nav");
    m_nav->setFocusPolicy(Qt::TabFocus);
    m_nav->addItems({tr("QR Scanner"), tr("QR Generator"), tr("File Explorer"), tr("Batch Processing"),
                     tr("Settings")});
    for (int i = 0; i < m_nav->count(); ++i)
        m_nav->item(i)->setToolTip(tr("Ctrl+%1").arg(i + 1));
    side->addWidget(m_nav, 1);
    auto *offline = makeLabel(tr("Runs locally · no network"), "muted");
    side->addWidget(offline);
    layout->addWidget(sidebar);

    m_stack = new QStackedWidget;
    m_scanner = new ScannerPage(m_settings);
    m_generator = new GeneratorPage(m_settings);
    m_files = new FilesPage(m_settings);
    m_batch = new BatchPage(m_settings);
    m_settingsPage = new SettingsPage(m_settings);
    for (QWidget *page : {static_cast<QWidget *>(m_scanner), static_cast<QWidget *>(m_generator),
                          static_cast<QWidget *>(m_files), static_cast<QWidget *>(m_batch),
                          static_cast<QWidget *>(m_settingsPage)})
        m_stack->addWidget(page);
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->showMessage(tr("Ready"));

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);
    for (int i = 0; i < m_nav->count(); ++i) {
        auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + i)), this);
        connect(shortcut, &QShortcut::activated, this, [this, i] { m_nav->setCurrentRow(i); });
    }
    auto *open = new QShortcut(QKeySequence::Open, this);
    connect(open, &QShortcut::activated, this, [this] {
        showPage(Scanner);
        m_scanner->chooseFile();
    });

    auto status = [this](const QString &message) { statusBar()->showMessage(message, 6000); };
    connect(m_scanner, &ScannerPage::statusMessage, this, status);
    connect(m_generator, &GeneratorPage::statusMessage, this, status);
    connect(m_files, &FilesPage::statusMessage, this, status);
    connect(m_batch, &BatchPage::statusMessage, this, status);
    connect(m_settingsPage, &SettingsPage::statusMessage, this, status);

    connect(m_files, &FilesPage::scanRequested, this, &MainWindow::scanFile);
    connect(m_batch, &BatchPage::scanRequested, this, &MainWindow::scanFile);
    connect(m_files, &FilesPage::batchRequested, this, [this](const QStringList &paths) {
        showPage(Batch);
        m_batch->scanFiles(paths);
    });
    connect(m_settingsPage, &SettingsPage::themeChanged, this, [this] {
        applyTheme(*qApp, m_settings->theme);
    });
    connect(m_settingsPage, &SettingsPage::generatorDefaultsChanged, m_generator, &GeneratorPage::applyDefaults);
}

void MainWindow::showPage(Page page) { m_nav->setCurrentRow(int(page)); }

void MainWindow::scanFile(const QString &path)
{
    showPage(Scanner);
    m_scanner->scanFile(path);
}

void MainWindow::generateFromText(const QString &text)
{
    showPage(Generator);
    m_generator->setText(text);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_batch->isRunning()) {
        const auto answer = QMessageBox::question(this, tr("Batch scan running"),
                                                  tr("A batch scan is still running. Cancel it and quit?"),
                                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_batch->cancel();
    }
    m_settings->save();
    event->accept();
}

} // namespace app
