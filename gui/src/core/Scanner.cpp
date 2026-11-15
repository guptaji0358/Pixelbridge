#include "qrs/Scanner.h"

#include "qrs/qrs_decode.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QStringDecoder>

#include <memory>

namespace qrs {

static QString tr(const char *s) { return QCoreApplication::translate("qrs", s); }

int ScanResult::decodedCount() const
{
    int n = 0;
    for (const QrCode &c : codes)
        n += c.decoded ? 1 : 0;
    return n;
}

QStringList ScanResult::texts() const
{
    QStringList out;
    for (const QrCode &c : codes)
        if (c.decoded)
            out << c.text;
    return out;
}

static QString payloadToText(const QByteArray &raw)
{
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8(raw);
    if (utf8.hasError())
        text = QString::fromLatin1(raw); // ISO-8859-1 is the QR byte-mode default
    return text;
}

ScanResult scanImage(const QImage &image)
{
    ScanResult result;
    QElapsedTimer timer;
    timer.start();

    if (image.isNull()) {
        result.status = ScanStatus::LoadError;
        result.error = tr("The image is empty.");
        return result;
    }
    result.imageSize = image.size();

    // Transparent pixels are composited onto white first; otherwise a QR code
    // on a transparent PNG background turns black-on-black.
    QImage gray;
    if (image.hasAlphaChannel()) {
        QImage flat(image.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        const QImage argb = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < argb.height(); ++y) {
            const QRgb *src = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
            QRgb *dst = reinterpret_cast<QRgb *>(flat.scanLine(y));
            for (int x = 0; x < argb.width(); ++x) {
                const int inv = 255 - qAlpha(src[x]);
                dst[x] = qRgb(qRed(src[x]) + inv, qGreen(src[x]) + inv, qBlue(src[x]) + inv);
            }
        }
        gray = flat.convertToFormat(QImage::Format_Grayscale8);
    } else {
        gray = image.convertToFormat(QImage::Format_Grayscale8);
    }

    constexpr int MaxResults = 32;
    std::unique_ptr<qrs_result[]> out(new qrs_result[MaxResults]);
    qrs_scan_stats stats{};
    const int n = qrs_decode_gray(gray.constBits(), gray.width(), gray.height(),
                                  int(gray.bytesPerLine()), QRS_STRAT_ALL, out.get(), MaxResults, &stats);

    for (unsigned bit = 1; bit <= QRS_STRAT_BLUR; bit <<= 1)
        if (stats.strategies_tried & bit)
            result.strategiesTried << QString::fromLatin1(qrs_strategy_name(bit));

    if (n < 0) {
        result.status = ScanStatus::LoadError;
        result.error = n == QRS_DEC_ERR_MEMORY ? tr("Not enough memory to scan this image.")
                                               : tr("The image could not be processed.");
        result.elapsedMs = timer.elapsed();
        return result;
    }

    for (int i = 0; i < n; ++i) {
        const qrs_result &r = out[i];
        QrCode code;
        code.decoded = r.status == 0;
        for (const qrs_point &p : r.corners)
            code.corners << QPoint(p.x, p.y);
        code.strategy = QString::fromLatin1(qrs_strategy_name(r.strategy));
        if (code.decoded) {
            code.raw = QByteArray(reinterpret_cast<const char *>(r.payload), r.payload_len);
            code.text = payloadToText(code.raw);
            code.version = r.version;
            code.eccLevel = r.ecc_level;
            code.mirrored = r.mirrored != 0;
        } else {
            code.failure = QString::fromLatin1(qrs_status_text(r.status));
        }
        result.codes << code;
    }

    result.status = result.decodedCount() > 0 ? ScanStatus::Found : ScanStatus::NoQr;
    result.elapsedMs = timer.elapsed();
    return result;
}

QImage loadImage(const QString &path, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QImage();
    };
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return fail(tr("File not found."));

    QImageReader reader(path);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true); // do not trust the extension
    const QSize size = reader.size();
    if (size.isValid() && (size.width() > MaxImageDimension || size.height() > MaxImageDimension))
        return fail(tr("Image is too large (%1 x %2). The limit is %3 pixels per side.")
                        .arg(size.width()).arg(size.height()).arg(MaxImageDimension));
    const QImage image = reader.read();
    if (image.isNull())
        return fail(tr("Not a readable image: %1").arg(reader.errorString()));
    return image;
}

ScanResult scanFile(const QString &path, QImage *loaded)
{
    ScanResult result;
    const QImage image = loadImage(path, &result.error);
    if (image.isNull()) {
        result.status = ScanStatus::LoadError;
    } else {
        result = scanImage(image);
        if (loaded)
            *loaded = image;
    }
    result.filePath = path;
    return result;
}

QStringList supportedImageExtensions()
{
    static const QStringList cached = [] {
        QStringList out;
        const auto formats = QImageReader::supportedImageFormats();
        for (const QByteArray &f : formats) {
            const QString ext = QString::fromLatin1(f).toLower();
            if (ext != QLatin1String("svg") && ext != QLatin1String("svgz") && ext != QLatin1String("pdf")
                && !out.contains(ext))
                out << ext;
        }
        out.sort();
        return out;
    }();
    return cached;
}

bool isSupportedImageFile(const QString &path)
{
    return supportedImageExtensions().contains(QFileInfo(path).suffix().toLower());
}

} // namespace qrs
