// Content rules, CSV export, file search and guarded file operations.
#include "TestUtil.h"
#include "qrs/Content.h"
#include "qrs/Files.h"

#include <QTemporaryDir>
#include <QTest>

using namespace qrs;

static void touch(const QString &path, const QByteArray &content = "x")
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

static QByteArray slurp(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

class FilesTest : public QObject {
    Q_OBJECT
private slots:
    // ---- URL handling ------------------------------------------------------
    void classify_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("canOpen");
        auto row = [](const char *name, const char *text, ContentKind kind, bool open) {
            QTest::newRow(name) << QString::fromUtf8(text) << int(kind) << open;
        };
        row("https", "https://example.com/a?b=c", ContentKind::Url, true);
        row("http", "http://example.com", ContentKind::Url, true);
        row("upper scheme", "HTTPS://EXAMPLE.COM", ContentKind::Url, true);
        row("www", "www.example.com/page", ContentKind::Url, true);
        row("plain text", "just some words", ContentKind::Text, false);
        row("text with url inside", "visit https://example.com now", ContentKind::Text, false);
        row("empty", "", ContentKind::Text, false);
        row("email", "someone@example.com", ContentKind::Email, true);
        row("mailto", "mailto:someone@example.com?subject=Hi", ContentKind::Email, true);
        row("tel", "tel:+15551234567", ContentKind::Phone, true);
        row("tel junk", "tel:abc", ContentKind::Phone, false);
        row("sms", "SMSTO:+15551234567:hello there", ContentKind::Sms, true);
        row("geo", "geo:48.85,2.35", ContentKind::Geo, true);
        row("wifi", "WIFI:T:WPA;S:net;P:secretpw;;", ContentKind::Wifi, false);
        row("vcard", "BEGIN:VCARD\nVERSION:3.0\nEND:VCARD", ContentKind::VCard, false);
        // Dangerous or unknown schemes must never be openable.
        row("javascript", "javascript:alert(1)", ContentKind::Url, false);
        row("file", "file:///C:/Windows/System32/calc.exe", ContentKind::Url, false);
        row("unc-ish", "\\\\server\\share\\run.exe", ContentKind::Text, false);
        row("custom scheme", "ms-settings:privacy", ContentKind::Url, false);
        row("data", "data:text/html,<script>1</script>", ContentKind::Url, false);
        row("no host", "https://", ContentKind::Url, false);
    }
    void classify()
    {
        QFETCH(QString, text);
        QFETCH(int, kind);
        QFETCH(bool, canOpen);
        const ContentInfo info = qrs::classify(text);
        QCOMPARE(int(info.kind), kind);
        QCOMPARE(info.canOpen(), canOpen);
        if (canOpen) {
            static const QStringList allowed = {"http", "https", "mailto", "tel", "sms", "geo"};
            QVERIFY(allowed.contains(info.openUrl.scheme().toLower()));
        }
    }

    void linkWarnings()
    {
        QVERIFY(qrs::classify("https://example.com").warning.isEmpty());
        QVERIFY(qrs::classify("http://example.com").warning.contains("not encrypted"));
        QVERIFY(qrs::classify("https://bank.com@evil.example/login").warning.contains("evil.example"));
        QVERIFY(!qrs::classify("javascript:alert(1)").warning.isEmpty());
    }

    void builders()
    {
        QString error;
        QCOMPARE(buildUrl("example.com/x", &error), QString("https://example.com/x"));
        QVERIFY(buildUrl("not a url", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(buildUrl("ftp://example.com").isEmpty());
        QCOMPARE(buildPhone("+1 (555) 123-4567"), QString("tel:+15551234567"));
        QVERIFY(buildPhone("12").isEmpty());
        QVERIFY(buildPhone("call me").isEmpty());
        QCOMPARE(buildSms("555 1234", "hi"), QString("SMSTO:5551234:hi"));
        QCOMPARE(buildEmail("a@b.co", "", ""), QString("mailto:a@b.co"));
        const QString mail = buildEmail("a@b.co", "Hello there", "Line & more");
        QVERIFY(mail.startsWith("mailto:a@b.co?subject=Hello%20there&body=Line%20%26%20more"));
        QVERIFY(buildEmail("nope", "", "").isEmpty());
    }

    void wifiBuildAndParse()
    {
        QString error;
        const QString payload = buildWifi("Caf\\é;Net:1", "p@ss;w\"ord,1", WifiSecurity::Wpa, true, &error);
        QVERIFY2(!payload.isEmpty(), qPrintable(error));
        QVERIFY(payload.startsWith("WIFI:T:WPA;S:"));
        QVERIFY(payload.endsWith(";;"));
        const WifiInfo info = parseWifi(payload);
        QVERIFY(info.valid);
        QCOMPARE(info.ssid, QString("Caf\\é;Net:1"));
        QCOMPARE(info.password, QString("p@ss;w\"ord,1"));
        QCOMPARE(info.security, QString("WPA"));
        QVERIFY(info.hidden);

        QCOMPARE(buildWifi("Open", "", WifiSecurity::Open, false), QString("WIFI:T:nopass;S:Open;;"));
        QVERIFY(buildWifi("", "password1", WifiSecurity::Wpa, false).isEmpty());
        QVERIFY(buildWifi("Net", "short", WifiSecurity::Wpa, false).isEmpty());
        QVERIFY(buildWifi(QString(40, 'n'), "password1", WifiSecurity::Wpa, false).isEmpty());
        QVERIFY(!parseWifi("not wifi").valid);
    }

    // ---- CSV ---------------------------------------------------------------
    void csvEscaping()
    {
        QCOMPARE(csvEscape("plain"), QString("plain"));
        QCOMPARE(csvEscape("a,b"), QString("\"a,b\""));
        QCOMPARE(csvEscape("say \"hi\""), QString("\"say \"\"hi\"\"\""));
        QCOMPARE(csvEscape("two\nlines"), QString("\"two\nlines\""));
        // Spreadsheet formula injection is neutralised.
        QCOMPARE(csvEscape("=HYPERLINK(\"http://x\")"), QString("\"'=HYPERLINK(\"\"http://x\"\")\""));
        QCOMPARE(csvEscape("+1"), QString("'+1"));
        QCOMPARE(csvEscape("-1"), QString("'-1"));
        QCOMPARE(csvEscape("@cmd"), QString("'@cmd"));
    }

    void csvExport()
    {
        QTemporaryDir dir;
        const QString two = dir.filePath("two.png");
        QVERIFY(testutil::compose({testutil::makeQr("alpha, with comma", 250), testutil::makeQr("https://b.example", 250)})
                    .save(two));
        const QString none = dir.filePath("none.png");
        QVERIFY(testutil::noise(200, 200).save(none));
        touch(dir.filePath("broken.png"), "garbage");

        QVector<ScanResult> results = {scanFile(two), scanFile(none), scanFile(dir.filePath("broken.png"))};
        const QString csvPath = dir.filePath("out.csv");
        QString error;
        QVERIFY2(writeCsv(csvPath, results, &error), qPrintable(error));

        const QByteArray bytes = slurp(csvPath);
        QVERIFY(bytes.startsWith("\xEF\xBB\xBF"));
        const QStringList lines = QString::fromUtf8(bytes.mid(3)).split("\r\n", Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 5); // header + 2 codes + no_qr + error
        QCOMPARE(lines[0], QString("file,folder,status,index,content,type,version,ecc,strategy,error"));
        QCOMPARE(lines.filter("two.png").size(), 2);
        QVERIFY(bytes.contains("\"alpha, with comma\""));
        QVERIFY(bytes.contains("https://b.example"));
        QCOMPARE(lines.filter(",no_qr,").size(), 1);
        QCOMPARE(lines.filter(",error,").size(), 1);

        QVERIFY(!writeCsv(dir.filePath("no/such/dir/out.csv"), results, &error));
    }

    // ---- search ------------------------------------------------------------
    void extensionParsing()
    {
        QCOMPARE(parseExtensions("png, .JPG;*.webp  png"), QStringList({"png", "jpg", "webp"}));
        QVERIFY(parseExtensions("  ,; ").isEmpty());
    }

    void searchAndFilter()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("sub/deep"));
        for (const char *name : {"Invoice_01.png", "invoice_02.JPG", "photo.jpeg", "notes.txt", "archive.zip",
                                 "sub/ticket.png", "sub/deep/Invoice_99.webp", "sub/readme.md"})
            touch(dir.filePath(name));

        auto names = [&](SearchQuery q) {
            QStringList out;
            for (const QFileInfo &f : findFiles(dir.path(), q))
                out << f.fileName();
            out.sort(Qt::CaseInsensitive);
            return out;
        };

        SearchQuery q;
        QCOMPARE(names(q), QStringList({"Invoice_01.png", "invoice_02.JPG", "photo.jpeg"}));
        q.imagesOnly = false;
        QCOMPARE(names(q).size(), 5);
        q.recursive = true;
        QCOMPARE(names(q).size(), 8);
        q.imagesOnly = true;
        q.nameContains = "INVOICE"; // case-insensitive
        QCOMPARE(names(q), QStringList({"Invoice_01.png", "invoice_02.JPG", "Invoice_99.webp"}));
        q.extensions = {"png"};
        QCOMPARE(names(q), QStringList({"Invoice_01.png"}));
        q.extensions.clear();
        q.nameContains = "inv*_0?";
        QCOMPARE(names(q), QStringList({"Invoice_01.png", "invoice_02.JPG"}));
        q.nameContains = "zzz";
        QVERIFY(names(q).isEmpty());

        // Extension filter also applies to non-images when imagesOnly is off.
        SearchQuery t;
        t.imagesOnly = false;
        t.extensions = {"txt"};
        QCOMPARE(names(t), QStringList({"notes.txt"}));

        QVERIFY(findFiles(dir.filePath("does-not-exist"), SearchQuery()).isEmpty());
        std::atomic_bool cancel{true};
        SearchQuery all;
        all.imagesOnly = false;
        QVERIFY(findFiles(dir.path(), all, &cancel).isEmpty());
        QCOMPARE(findFiles(dir.path(), all, nullptr, 2).size(), 2);
    }

    // ---- file-operation safety ---------------------------------------------
    void moveNeverOverwrites()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("src"));
        QVERIFY(QDir(dir.path()).mkpath("dst"));
        touch(dir.filePath("src/a.png"), "new-a");
        touch(dir.filePath("src/b.png"), "new-b");
        touch(dir.filePath("dst/a.png"), "old-a");
        touch(dir.filePath("dst/a (2).png"), "old-a2");

        const FileOpReport report =
            moveFiles({dir.filePath("src/a.png"), dir.filePath("src/b.png"), dir.filePath("src/ghost.png")},
                      dir.filePath("dst"));
        QCOMPARE(report.succeeded.size(), 2);
        QCOMPARE(report.failed.size(), 1);
        QVERIFY(report.failed[0].contains("ghost.png"));
        QCOMPARE(slurp(dir.filePath("dst/a.png")), QByteArray("old-a"));        // untouched
        QCOMPARE(slurp(dir.filePath("dst/a (2).png")), QByteArray("old-a2"));   // untouched
        QCOMPARE(slurp(dir.filePath("dst/a (3).png")), QByteArray("new-a"));
        QCOMPARE(slurp(dir.filePath("dst/b.png")), QByteArray("new-b"));
        QVERIFY(!QFile::exists(dir.filePath("src/a.png")));
        QVERIFY(!QFile::exists(dir.filePath("src/b.png")));
    }

    void moveRejectsBadTargets()
    {
        QTemporaryDir dir;
        touch(dir.filePath("keep.png"), "keep");
        FileOpReport same = moveFiles({dir.filePath("keep.png")}, dir.path());
        QCOMPARE(same.failed.size(), 1);
        FileOpReport missing = moveFiles({dir.filePath("keep.png")}, dir.filePath("nope"));
        QCOMPARE(missing.failed.size(), 1);
        FileOpReport folder = moveFiles({dir.path()}, QDir::tempPath()); // folders are not moved
        QCOMPARE(folder.failed.size(), 1);
        QCOMPARE(slurp(dir.filePath("keep.png")), QByteArray("keep"));
    }

    void trashMissingFileFails()
    {
        QTemporaryDir dir;
        const FileOpReport report = moveToTrash({dir.filePath("ghost.png")});
        QVERIFY(report.succeeded.isEmpty());
        QCOMPARE(report.failed.size(), 1);
    }

    // Real Windows test: puts one small temporary file in the user's Recycle
    // Bin, so it only runs when QRS_TEST_RECYCLE_BIN=1.
    void trashRealFile()
    {
        if (qEnvironmentVariable("QRS_TEST_RECYCLE_BIN") != "1")
            QSKIP("set QRS_TEST_RECYCLE_BIN=1 to send a temp file to the real Recycle Bin");
        QTemporaryDir dir;
        const QString path = dir.filePath("qrstudio-recycle-test.png");
        touch(path);
        const FileOpReport report = moveToTrash({path});
        QVERIFY2(report.ok(), qPrintable(report.failed.join('\n')));
        QVERIFY(!QFile::exists(path));
    }
};

QTEST_GUILESS_MAIN(FilesTest)
#include "tst_files.moc"
