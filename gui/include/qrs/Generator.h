// QR generation: Rust encoder -> module matrix -> rendered QImage.
#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QVector>

namespace qrs {

enum class EccLevel { Low = 0, Medium = 1, Quartile = 2, High = 3 };

struct QrMatrix {
    int size = 0;            // modules per side; 0 = invalid
    QVector<quint8> modules; // row-major, 1 = dark
    bool isValid() const { return size > 0; }
    bool dark(int x, int y) const { return modules[y * size + x] != 0; }
};

struct GenerateOptions {
    int pixelSize = 512;               // output side length in pixels (incl. margin)
    int marginModules = 4;             // quiet zone, 0..16 (spec recommends 4)
    QColor foreground = Qt::black;
    QColor background = Qt::white;
    EccLevel ecc = EccLevel::Medium;
};

struct GenerateResult {
    QImage image;      // null on failure
    QrMatrix matrix;
    QString error;     // empty on success
    QString warning;   // non-fatal (e.g. low contrast)
    bool ok() const { return !image.isNull(); }
};

namespace limits {
constexpr int MinPixelSize = 64;
constexpr int MaxPixelSize = 4096;
constexpr int MaxMargin = 16;
}

// Encodes UTF-8 bytes into a module matrix. `error` receives a user-facing
// message on failure.
QrMatrix encodeMatrix(const QByteArray &utf8, EccLevel ecc, QString *error = nullptr);

// Renders with crisp integer-sized modules; any leftover pixels become extra
// background so the image is exactly pixelSize x pixelSize.
QImage renderMatrix(const QrMatrix &matrix, const GenerateOptions &options);

// Full pipeline with input validation.
GenerateResult generate(const QString &text, const GenerateOptions &options);

// WCAG-style contrast ratio (1..21) between two colours.
double contrastRatio(const QColor &a, const QColor &b);

// Scalable output for export.
QByteArray renderSvg(const QrMatrix &matrix, const GenerateOptions &options);

} // namespace qrs
