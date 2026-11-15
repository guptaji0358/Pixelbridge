// Widget-level tests of the application pages (real widgets, real clipboard).
// Set QRS_SCREENSHOT_DIR to save a PNG of every page for visual review.
#include "BatchPage.h"
#include "FilesPage.h"
#include "GeneratorPage.h"
#include "MainWindow.h"
#include "ScannerPage.h"
#include "Theme.h"
#include "Widgets.h"
#include "qrs/Generator.h"
#include "qrs/Scanner.h"

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

using namespace app;

static QPushButton *button(QWidget *root, const QString &text)
{
    for (QPushButton *b : root->findChildren<QPushButton *>())
        if (b->text() == text)
            return b;
    return nullptr;
}

static bool hasLabel(QWidget *root, const QString &text)
{
    for (QLabel *l : root->findChildren<QLabel *>())
        if (l->isVisibleTo(root) && l->text().contains(text))
            return true;
    return false;
}

static QImage qr(const QString &text)
{
    qrs::GenerateOptions o;
    o.pixelSize = 300;
    return qrs::generate(text, o).image;
}

class GuiTest : public QObject {
    Q_OBJECT
    Settings m_settings;
    MainWindow *m_window = nullptr;
    QTemporaryDir m_dir;

    void shot(const QString &name)
    {
        const QString dir = qEnvironmentVariable("QRS_SCREENSHOT_DIR");
        if (dir.isEmpty())
            return;
        QTest::qWait(150);
        m_window->grab().save(QDir(dir).filePath(name + ".png"));
    }
    template <typename T> T *page() { return m_window->findChild<T *>(); }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true); // never touch the real settings file
        m_settings.theme = qEnvironmentVariable("QRS_TEST_THEME") == "dark" ? ThemeMode::Dark : ThemeMode::Light;
        m_settings.lastBrowseDir = m_dir.path();
        applyTheme(*qApp, m_settings.theme);
        QVERIFY(qr("https://example.com/menu").save(m_dir.filePath("menu-link.png")));
        QVERIFY(qr("Table 12").save(m_dir.filePath("table-12.jpg")));
        QImage blank(300, 200, QImage::Format_RGB32);
        blank.fill(Qt::lightGray);
        QVERIFY(blank.save(m_dir.filePath("no-code-here.png")));
        QFile notes(m_dir.filePath("notes.txt"));
        QVERIFY(notes.open(QIODevice::WriteOnly));
        notes.write("hello");
        notes.close();

        m_window = new MainWindow(&m_settings);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
    }
    void cleanupTestCase() { delete m_window; }

    void navigationHasFivePages()
    {
        auto *nav = m_window->findChild<QListWidget *>("nav");
        QVERIFY(nav);
        QCOMPARE(nav->count(), 5);
        for (int i = 0; i < 5; ++i) {
            m_window->showPage(MainWindow::Page(i));
            QCOMPARE(nav->currentRow(), i);
        }
    }

    void scannerDecodesAndCopies()
    {
        auto *scanner = page<ScannerPage>();
        m_window->showPage(MainWindow::Scanner);
        shot("1-scanner-empty");
        QVERIFY(!button(scanner, "Copy")->isEnabled());
        QVERIFY(!button(scanner, "Open link")->isEnabled());

        m_window->scanFile(m_dir.filePath("menu-link.png"));
        QTRY_VERIFY_WITH_TIMEOUT(hasLabel(scanner, "QR code found"), 10000);
        auto *text = scanner->findChild<QPlainTextEdit *>();
        QCOMPARE(text->toPlainText(), QString("https://example.com/menu"));
        QVERIFY(button(scanner, "Copy")->isEnabled());
        QVERIFY(button(scanner, "Open link")->isEnabled()); // enabled, but never clicked by tests
        QVERIFY(button(scanner, "Export…")->isEnabled());

        QApplication::clipboard()->setText("stale");
        QTest::mouseClick(button(scanner, "Copy"), Qt::LeftButton);
        QCOMPARE(QApplication::clipboard()->text(), QString("https://example.com/menu"));
        shot("2-scanner-found");
    }

    void scannerNoQrDisablesActions()
    {
        auto *scanner = page<ScannerPage>();
        m_window->scanFile(m_dir.filePath("no-code-here.png"));
        QTRY_VERIFY_WITH_TIMEOUT(hasLabel(scanner, "No QR found"), 10000);
        QVERIFY(!button(scanner, "Copy")->isEnabled());
        QVERIFY(!button(scanner, "Open link")->isEnabled());
        QVERIFY(!button(scanner, "Export…")->isEnabled());
        QVERIFY(scanner->findChild<QPlainTextEdit *>()->toPlainText().isEmpty());
        shot("3-scanner-no-qr");

        m_window->scanFile(m_dir.filePath("notes.txt"));
        QTRY_VERIFY_WITH_TIMEOUT(hasLabel(scanner, "Could not read the image"), 10000);
        QVERIFY(!button(scanner, "Copy")->isEnabled());
    }

    void scannerPlainTextIsNotOpenable()
    {
        auto *scanner = page<ScannerPage>();
        m_window->scanFile(m_dir.filePath("table-12.jpg"));
        QTRY_VERIFY_WITH_TIMEOUT(hasLabel(scanner, "QR code found"), 10000);
        QTRY_COMPARE(scanner->findChild<QPlainTextEdit *>()->toPlainText(), QString("Table 12"));
        QVERIFY(button(scanner, "Copy")->isEnabled());
        QVERIFY(!button(scanner, "Open link")->isEnabled());
    }

    void scannerPastesClipboardImage()
    {
        auto *scanner = page<ScannerPage>();
        m_window->showPage(MainWindow::Scanner);
        QApplication::clipboard()->setImage(qr("from clipboard"));
        QTest::mouseClick(button(scanner, "Paste"), Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(scanner->findChild<QPlainTextEdit *>()->toPlainText(),
                                  QString("from clipboard"), 10000);
    }

    void generatorPreviewAndClipboard()
    {
        auto *gen = page<GeneratorPage>();
        m_window->showPage(MainWindow::Generator);
        gen->setText(QString());
        QVERIFY(!button(gen, "Save…")->isEnabled());
        QVERIFY(!button(gen, "Copy image")->isEnabled());
        shot("4-generator-empty");

        m_window->generateFromText("generated in a test");
        QVERIFY(button(gen, "Save…")->isEnabled());
        auto *preview = gen->findChild<ImageView *>();
        QVERIFY(!preview->image().isNull());

        QApplication::clipboard()->clear();
        QTest::mouseClick(button(gen, "Copy image"), Qt::LeftButton);
        const QImage copied = QApplication::clipboard()->image();
        QVERIFY(!copied.isNull());
        QCOMPARE(qrs::scanImage(copied).texts(), QStringList{"generated in a test"});

        QTest::mouseClick(button(gen, "Copy text"), Qt::LeftButton);
        QCOMPARE(QApplication::clipboard()->text(), QString("generated in a test"));
        shot("5-generator-preview");

        gen->setText(QString(4000, 'x')); // too long: clear error, actions disabled
        QVERIFY(!button(gen, "Save…")->isEnabled());
        QVERIFY(hasLabel(gen, "too long"));
        gen->setText("ok again");
        QVERIFY(button(gen, "Save…")->isEnabled());
    }

    void filesLiveSearch()
    {
        auto *files = page<FilesPage>();
        m_window->showPage(MainWindow::Files);
        auto *table = files->findChild<QTableView *>();
        QTRY_COMPARE_WITH_TIMEOUT(table->model()->rowCount(), 3, 10000); // images only
        QVERIFY(!button(files, "Delete…")->isEnabled());                 // nothing selected
        QVERIFY(!button(files, "Move…")->isEnabled());

        QLineEdit *search = nullptr;
        for (QLineEdit *e : files->findChildren<QLineEdit *>())
            if (e->placeholderText().startsWith("Search"))
                search = e;
        QVERIFY(search);
        search->setText("TABLE");
        QTRY_COMPARE(table->model()->rowCount(), 1);
        search->setText("zzz-nothing");
        QTRY_COMPARE(table->model()->rowCount(), 0);
        QVERIFY(hasLabel(files, "No files match"));
        search->clear();
        QTRY_COMPARE(table->model()->rowCount(), 3);

        table->selectAll(); // multiple selection
        QVERIFY(button(files, "Batch scan selected")->isEnabled());
        QVERIFY(button(files, "Delete…")->isEnabled());
        QVERIFY(!button(files, "Scan")->isEnabled()); // needs exactly one file
        shot("6-files");
        table->clearSelection();
    }

    void batchScansFolder()
    {
        auto *batch = page<BatchPage>();
        m_window->showPage(MainWindow::Batch);
        shot("7-batch-empty");
        batch->scanFiles({m_dir.filePath("menu-link.png"), m_dir.filePath("table-12.jpg"),
                          m_dir.filePath("no-code-here.png"), m_dir.filePath("notes.txt")});
        QTRY_VERIFY_WITH_TIMEOUT(!batch->isRunning(), 20000);
        auto *table = batch->findChild<QTableWidget *>();
        QCOMPARE(table->rowCount(), 4);
        QStringList statuses;
        for (int r = 0; r < 4; ++r)
            statuses << table->item(r, 1)->text();
        QCOMPARE(statuses.count("QR found"), 2); // exact match: "No QR found" contains "QR found"
        QCOMPARE(statuses.count("No QR found"), 1);
        QCOMPARE(statuses.count("Error"), 1);
        QVERIFY(button(batch, "Export CSV…")->isEnabled());
        QVERIFY(hasLabel(batch, "4 scanned"));
        shot("8-batch-results");
    }

    void settingsPageShows()
    {
        m_window->showPage(MainWindow::SettingsTab);
        QTest::qWait(100);
        QVERIFY(hasLabel(m_window, "FILE EXPLORER RIGHT-CLICK MENU"));
        shot("9-settings");
        m_window->showPage(MainWindow::Scanner);
    }
};

QTEST_MAIN(GuiTest)
#include "tst_gui.moc"
