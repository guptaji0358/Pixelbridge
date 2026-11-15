#include "qrs/Generator.h"

#include "qrs/qrs_encode.h"

#include <QCoreApplication>
#include <QPainter>
#include <QtMath>

#include <algorithm>

namespace qrs {

static QString tr(const char *s) { return QCoreApplication::translate("qrs", s); }

QrMatrix encodeMatrix(const QByteArray &utf8, EccLevel ecc, QString *error)
{
    QrMatrix m;
    QVector<quint8> buf(QRS_ENCODE_MAX_SIZE * QRS_ENCODE_MAX_SIZE);
    const int rc = qrs_encode(reinterpret_cast<const uint8_t *>(utf8.constData()),
                              size_t(utf8.size()), int(ecc), buf.data(), size_t(buf.size()));
    if (rc > 0) {
        buf.resize(rc * rc);
        m.size = rc;
        m.modules = buf;
        return m;
    }
    if (error) {
        switch (rc) {
        case QRS_ENC_ERR_TOO_LONG:
            *error = tr("The data is too long for a QR code at this error-correction level "
                        "(%1 bytes). Shorten it or lower the error correction.").arg(utf8.size());
            break;
        case QRS_ENC_ERR_ECL:
            *error = tr("Invalid error-correction level.");
            break;
        default:
            *error = tr("The encoder failed (code %1).").arg(rc);
        }
    }
    return m;
}

QImage renderMatrix(const QrMatrix &matrix, const GenerateOptions &o)
{
    if (!matrix.isValid())
        return {};
    const int margin = qBound(0, o.marginModules, limits::MaxMargin);
    const int total = matrix.size + 2 * margin;
    const int px = qBound(limits::MinPixelSize, o.pixelSize, limits::MaxPixelSize);
    const int scale = qMax(1, px / total);
    // If the requested size cannot fit one pixel per module, grow to the minimum.
    const int side = qMax(px, total * scale);
    const int offset = (side - matrix.size * scale) / 2;

    QImage img(side, side, QImage::Format_RGB32);
    img.fill(o.background.rgb());
    const QRgb fg = o.foreground.rgb();
    for (int y = 0; y < matrix.size; ++y) {
        for (int x = 0; x < matrix.size; ++x) {
            if (!matrix.dark(x, y))
                continue;
            for (int dy = 0; dy < scale; ++dy) {
                QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(offset + y * scale + dy));
                std::fill_n(line + offset + x * scale, scale, fg);
            }
        }
    }
    return img;
}

static double luminance(const QColor &c)
{
    auto ch = [](double v) { return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

GenerateResult generate(const QString &text, const GenerateOptions &options)
{
    GenerateResult r;
    if (text.isEmpty()) {
        r.error = tr("Enter some text or data to encode.");
        return r;
    }
    if (!options.foreground.isValid() || !options.background.isValid()) {
        r.error = tr("Invalid colour.");
        return r;
    }
    if (options.foreground.rgb() == options.background.rgb()) {
        r.error = tr("Foreground and background colours are identical; the code would be invisible.");
        return r;
    }
    r.matrix = encodeMatrix(text.toUtf8(), options.ecc, &r.error);
    if (!r.matrix.isValid())
        return r;
    r.image = renderMatrix(r.matrix, options);

    const double ratio = contrastRatio(options.foreground, options.background);
    if (ratio < 3.0) {
        r.warning = tr("Low contrast (%1:1). Many scanners will fail to read this code.")
                        .arg(ratio, 0, 'f', 1);
    } else if (luminance(options.foreground) > luminance(options.background)) {
        r.warning = tr("Light-on-dark codes are not readable by every scanner.");
    } else if (options.marginModules < 2) {
        r.warning = tr("A margin under 2 modules can make the code hard to scan.");
    }
    return r;
}

QByteArray renderSvg(const QrMatrix &matrix, const GenerateOptions &o)
{
    if (!matrix.isValid())
        return {};
    const int margin = qBound(0, o.marginModules, limits::MaxMargin);
    const int total = matrix.size + 2 * margin;
    QByteArray svg;
    svg += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + QByteArray::number(total) + ' '
           + QByteArray::number(total) + "\" width=\"" + QByteArray::number(o.pixelSize)
           + "\" height=\"" + QByteArray::number(o.pixelSize) + "\" shape-rendering=\"crispEdges\">\n";
    svg += "<rect width=\"100%\" height=\"100%\" fill=\"" + o.background.name().toLatin1() + "\"/>\n";
    svg += "<path fill=\"" + o.foreground.name().toLatin1() + "\" d=\"";
    for (int y = 0; y < matrix.size; ++y) {
        int x = 0;
        while (x < matrix.size) {
            if (!matrix.dark(x, y)) {
                ++x;
                continue;
            }
            int run = 1;
            while (x + run < matrix.size && matrix.dark(x + run, y))
                ++run;
            svg += 'M' + QByteArray::number(x + margin) + ' ' + QByteArray::number(y + margin) + 'h'
                   + QByteArray::number(run) + "v1h-" + QByteArray::number(run) + 'z';
            x += run;
        }
    }
    svg += "\"/>\n</svg>\n";
    return svg;
}

} // namespace qrs
