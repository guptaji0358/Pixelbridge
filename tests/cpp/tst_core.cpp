// Encode / decode tests. Every round trip crosses all three languages:
// Rust encoder (DLL) -> C++ renderer -> C decoder (quirc).
#include "TestUtil.h"
#include "qrs/qrs_decode.h"
#include "qrs/qrs_encode.h"

#include <QTemporaryDir>
#include <QTest>

using namespace qrs;
using namespace testutil;

class CoreTest : public QObject {
    Q_OBJECT
private slots:
    // ---- interop: Rust ABI ------------------------------------------------
    void rustAbiVersionMatchesHeader() { QCOMPARE(qrs_encode_abi_version(), QRS_ENCODE_ABI_VERSION); }

    void rustRejectsBadArguments()
    {
        uint8_t out[32];
        QCOMPARE(qrs_encode(nullptr, 4, QRS_ECL_M, out, sizeof out), int32_t(QRS_ENC_ERR_NULL));
        QCOMPARE(qrs_encode((const uint8_t *)"abc", 3, 7, out, sizeof out), int32_t(QRS_ENC_ERR_ECL));
        QCOMPARE(qrs_encode((const uint8_t *)"abc", 3, QRS_ECL_M, out, sizeof out), int32_t(QRS_ENC_ERR_BUFFER));
    }

    // ---- interop: C decoder ABI --------------------------------------------
    void decoderRejectsBadArguments()
    {
        qrs_result r;
        uint8_t px[4] = {0};
        QCOMPARE(qrs_decode_gray(nullptr, 2, 2, 2, QRS_STRAT_ALL, &r, 1, nullptr), int(QRS_DEC_ERR_ARG));
        QCOMPARE(qrs_decode_gray(px, 2, 2, 1, QRS_STRAT_ALL, &r, 1, nullptr), int(QRS_DEC_ERR_ARG));
        QCOMPARE(qrs_decode_gray(px, 2, 2, 2, QRS_STRAT_ALL, &r, 0, nullptr), int(QRS_DEC_ERR_ARG));
        QCOMPARE(qrs_decode_gray(px, 2, 2, 2, QRS_STRAT_ALL, &r, 1, nullptr), 0);
    }

    // ---- generation -------------------------------------------------------
    void roundTrip_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("plain") << "Hello, QR Studio!";
        QTest::newRow("url") << "https://example.com/path?q=1&r=two#frag";
        QTest::newRow("numeric") << "0123456789012345678901234567890";
        QTest::newRow("alphanumeric") << "HELLO WORLD $%*+-./:";
        QTest::newRow("unicode") << QString::fromUtf8("नमस्ते दुनिया — こんにちは 😀");
        QTest::newRow("wifi") << "WIFI:T:WPA;S:My\\;Net;P:p\\:ss\\\\word;;";
        QTest::newRow("multiline") << "line one\nline two\r\n\ttabbed";
        QTest::newRow("single char") << "x";
        QTest::newRow("long") << QString(1200, QChar('k')); // must fit ECC H (max 1273 bytes)
    }
    void roundTrip()
    {
        QFETCH(QString, text);
        for (int ecc = 0; ecc < 4; ++ecc) {
            const QImage img = makeQr(text, 600, EccLevel(ecc));
            QVERIFY(!img.isNull());
            const ScanResult r = scanImage(img);
            QCOMPARE(r.status, ScanStatus::Found);
            QCOMPARE(r.codes.size(), 1);
            QCOMPARE(r.codes[0].text, text);
            QCOMPARE(r.codes[0].raw, text.toUtf8());
            QCOMPARE(r.codes[0].eccLevel, ecc); // regression: quirc's M/L/H/Q order was reported raw
        }
    }

    void everyVersionDecodes()
    {
        // Byte capacity of version v at ECC M grows monotonically; fill each version.
        int lastSize = 0;
        for (int len = 10; len <= 2300; len += (len < 400 ? 13 : 61)) {
            QByteArray data(len, 'a');
            for (int i = 0; i < len; ++i)
                data[i] = char('a' + (i * 7) % 26);
            QString error;
            const QrMatrix m = encodeMatrix(data, EccLevel::Medium, &error);
            QVERIFY2(m.isValid(), qPrintable(error));
            QVERIFY(m.size >= lastSize);
            lastSize = m.size;
            GenerateOptions o;
            o.pixelSize = (m.size + 8) * 4;
            const ScanResult r = scanImage(renderMatrix(m, o));
            QVERIFY2(r.status == ScanStatus::Found, qPrintable(QString("len %1 size %2").arg(len).arg(m.size)));
            QCOMPARE(r.codes[0].raw, data);
            QCOMPARE(r.codes[0].version, (m.size - 17) / 4);
        }
        QCOMPARE(lastSize, 177); // reached version 40
    }

    void renderGeometryAndColours()
    {
        GenerateOptions o;
        o.pixelSize = 500;
        o.marginModules = 2;
        o.foreground = QColor("#0b3d91");
        o.background = QColor("#fff7e0");
        const GenerateResult g = generate("colour test", o);
        QVERIFY(g.ok());
        QCOMPARE(g.image.size(), QSize(500, 500));
        QCOMPARE(g.image.pixelColor(0, 0), o.background);
        int fg = 0;
        for (int y = 0; y < 500; y += 3)
            for (int x = 0; x < 500; x += 3)
                fg += g.image.pixelColor(x, y) == o.foreground ? 1 : 0;
        QVERIFY(fg > 1000);
        QCOMPARE(scanImage(g.image).texts(), QStringList{"colour test"});
    }

    void tinySizeRequestStillScannable()
    {
        GenerateOptions o;
        o.pixelSize = 64;
        const GenerateResult g = generate(QString(600, 'z'), o); // far more modules than 64 px
        QVERIFY(g.ok());
        QVERIFY(g.image.width() >= g.matrix.size + 8);
    }

    void validationErrors()
    {
        GenerateOptions o;
        QVERIFY(!generate("", o).ok());
        QVERIFY(!generate(QString(3000, 'x'), o).ok());
        QVERIFY(generate(QString(3000, 'x'), o).error.contains("too long"));
        o.foreground = o.background = Qt::red;
        QVERIFY(!generate("abc", o).ok());
        o.foreground = QColor("#777777");
        o.background = QColor("#888888");
        const GenerateResult low = generate("abc", o);
        QVERIFY(low.ok());
        QVERIFY(low.warning.contains("contrast"));
    }

    void svgOutput()
    {
        GenerateOptions o;
        const GenerateResult g = generate("svg", o);
        const QByteArray svg = renderSvg(g.matrix, o);
        QVERIFY(svg.startsWith("<?xml"));
        QVERIFY(svg.contains("viewBox=\"0 0 29 29\"")); // 21 modules + 2 * 4 margin
        QVERIFY(svg.contains("<path"));
    }

    // ---- scanning ----------------------------------------------------------
    void noQrInBlankAndNoise()
    {
        QImage blank(400, 300, QImage::Format_RGB32);
        blank.fill(Qt::white);
        ScanResult r = scanImage(blank);
        QCOMPARE(r.status, ScanStatus::NoQr);
        QCOMPARE(r.decodedCount(), 0);
        QVERIFY(r.strategiesTried.size() >= 3); // every applicable pass was attempted
        QCOMPARE(scanImage(noise(320, 240)).status, ScanStatus::NoQr);
        QCOMPARE(scanImage(QImage()).status, ScanStatus::LoadError);
    }

    void damagedCodeIsLocatedButNotDecoded()
    {
        QImage img = makeQr("damaged beyond repair", 400, EccLevel::Low);
        QPainter p(&img);
        p.fillRect(150, 150, 190, 190, Qt::white); // wipe the data area, keep the finders
        p.end();
        const ScanResult r = scanImage(img);
        QCOMPARE(r.status, ScanStatus::NoQr);
        if (!r.codes.isEmpty()) { // candidate region reported for highlighting
            QVERIFY(!r.codes[0].decoded);
            QCOMPARE(r.codes[0].corners.size(), 4);
            QVERIFY(!r.codes[0].failure.isEmpty());
        }
    }

    void multipleCodes()
    {
        const QStringList texts = {"first code", "https://second.example", "THIRD 333"};
        QVector<QImage> images;
        for (const QString &t : texts)
            images << makeQr(t, 260);
        const ScanResult r = scanImage(compose(images));
        QCOMPARE(r.status, ScanStatus::Found);
        QCOMPARE(textSet(r), QSet<QString>(texts.begin(), texts.end()));
        for (const QrCode &c : r.codes) // corners must lie inside the image
            for (const QPoint &pt : c.corners)
                QVERIFY(QRect(QPoint(0, 0), r.imageSize).adjusted(-2, -2, 2, 2).contains(pt));
    }

    void transformedInputs_data()
    {
        QTest::addColumn<QImage>("image");
        const QImage base = compose({makeQr("transform me", 320)}, 60);
        QTest::newRow("rotated 90") << base.transformed(QTransform().rotate(90));
        QTest::newRow("rotated 33") << [&] {
            QImage canvas(700, 700, QImage::Format_RGB32);
            canvas.fill(Qt::white);
            QPainter p(&canvas);
            p.setRenderHint(QPainter::SmoothPixmapTransform);
            p.translate(350, 350);
            p.rotate(33);
            p.drawImage(-base.width() / 2, -base.height() / 2, base);
            return canvas;
        }();
        QTest::newRow("mirrored") << base.flipped(Qt::Horizontal);
        QTest::newRow("inverted") << [&] { QImage i = base; i.invertPixels(); return i; }();
        QTest::newRow("tiny 2px modules") << compose({makeQr("transform me", 64)}, 10);
        QTest::newRow("low contrast") << [&] {
            QImage i = base.convertToFormat(QImage::Format_Grayscale8);
            for (int y = 0; y < i.height(); ++y)
                for (int x = 0; x < i.width(); ++x)
                    i.scanLine(y)[x] = uchar(110 + i.scanLine(y)[x] / 8);
            return i;
        }();
        QTest::newRow("transparent background") << [&] {
            QImage i(base.size(), QImage::Format_ARGB32);
            i.fill(Qt::transparent);
            for (int y = 0; y < base.height(); ++y)
                for (int x = 0; x < base.width(); ++x)
                    if (qGray(base.pixel(x, y)) < 128)
                        i.setPixel(x, y, qRgba(0, 0, 0, 255));
            return i;
        }();
        QTest::newRow("large 4000x3000") << [&] {
            QImage canvas(4000, 3000, QImage::Format_RGB32);
            canvas.fill(QColor("#e8e8e8"));
            QPainter p(&canvas);
            p.drawImage(2500, 1700, makeQr("transform me", 900));
            return canvas;
        }();
    }
    void transformedInputs()
    {
        QFETCH(QImage, image);
        const ScanResult r = scanImage(image);
        QVERIFY2(r.status == ScanStatus::Found, qPrintable("passes: " + r.strategiesTried.join(',')));
        QCOMPARE(r.codes[0].text, QString("transform me"));
    }

    void imageFormats()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QImage img = makeQr("format check", 400);
        for (const char *ext : {"png", "jpg", "jpeg", "bmp", "webp", "tif", "gif"}) {
            if (!supportedImageExtensions().contains(ext))
                continue;
            const QString path = dir.filePath(QString("code.") + ext);
            if (!img.save(path)) // some formats are read-only in this Qt build
                continue;
            const ScanResult r = scanFile(path);
            QVERIFY2(r.status == ScanStatus::Found, ext);
            QCOMPARE(r.texts(), QStringList{"format check"});
            QCOMPARE(r.filePath, path);
        }
        QVERIFY(isSupportedImageFile("C:/x/photo.PNG"));
        QVERIFY(!isSupportedImageFile("C:/x/notes.txt"));
    }

    void invalidFiles()
    {
        QTemporaryDir dir;
        QCOMPARE(scanFile(dir.filePath("missing.png")).status, ScanStatus::LoadError);
        QFile fake(dir.filePath("not-an-image.png"));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("this is plain text, not a PNG");
        fake.close();
        const ScanResult r = scanFile(fake.fileName());
        QCOMPARE(r.status, ScanStatus::LoadError);
        QVERIFY(!r.error.isEmpty());
        QCOMPARE(scanFile(dir.path()).status, ScanStatus::LoadError); // a folder
        // A PNG saved with the wrong extension is still read (content sniffing).
        const QString wrong = dir.filePath("actually-png.jpg");
        QVERIFY(makeQr("sniffed").save(wrong, "PNG"));
        QCOMPARE(scanFile(wrong).texts(), QStringList{"sniffed"});
    }
};

QTEST_GUILESS_MAIN(CoreTest)
#include "tst_core.moc"
