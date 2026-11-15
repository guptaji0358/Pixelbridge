// Shared helpers for the C++ tests.
#pragma once

#include "qrs/Generator.h"
#include "qrs/Scanner.h"

#include <QImage>
#include <QPainter>
#include <QSet>

namespace testutil {

inline QImage makeQr(const QString &text, int pixels = 300, qrs::EccLevel ecc = qrs::EccLevel::Medium)
{
    qrs::GenerateOptions o;
    o.pixelSize = pixels;
    o.ecc = ecc;
    return qrs::generate(text, o).image;
}

// Places several images side by side on a white canvas with padding.
inline QImage compose(const QVector<QImage> &images, int pad = 30)
{
    int width = pad, height = 0;
    for (const QImage &i : images) {
        width += i.width() + pad;
        height = qMax(height, i.height());
    }
    QImage canvas(width, height + 2 * pad, QImage::Format_RGB32);
    canvas.fill(Qt::white);
    QPainter p(&canvas);
    int x = pad;
    for (const QImage &i : images) {
        p.drawImage(x, pad, i);
        x += i.width() + pad;
    }
    return canvas;
}

inline QSet<QString> textSet(const qrs::ScanResult &r)
{
    const QStringList texts = r.texts();
    return QSet<QString>(texts.begin(), texts.end());
}

// Deterministic pseudo-random noise image (no QR inside).
inline QImage noise(int w, int h, quint32 seed = 12345)
{
    QImage img(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        uchar *line = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            seed = seed * 1664525u + 1013904223u;
            line[x] = uchar(seed >> 24);
        }
    }
    return img;
}

} // namespace testutil
