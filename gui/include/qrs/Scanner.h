// QR scanning: QImage / file -> grayscale -> C decoding core.
#pragma once

#include <QImage>
#include <QPolygon>
#include <QString>
#include <QStringList>
#include <QVector>

namespace qrs {

struct QrCode {
    bool decoded = false;
    QString text;        // decoded payload as text (UTF-8, Latin-1 fallback)
    QByteArray raw;      // exact payload bytes
    QPolygon corners;    // 4 points in original image coordinates
    int version = 0;
    int eccLevel = 0;    // 0 L, 1 M, 2 Q, 3 H
    bool mirrored = false;
    QString strategy;    // preprocessing pass that found it
    QString failure;     // reason when !decoded
};

enum class ScanStatus {
    Found,       // at least one symbol decoded
    NoQr,        // image loaded fine, nothing decoded
    LoadError    // file missing / unreadable / not an image
};

struct ScanResult {
    ScanStatus status = ScanStatus::NoQr;
    QString filePath;        // empty for in-memory scans
    QString error;           // set for LoadError
    QSize imageSize;
    QVector<QrCode> codes;   // decoded symbols, or undecodable candidates when status == NoQr
    QStringList strategiesTried;
    qint64 elapsedMs = 0;

    int decodedCount() const;
    QStringList texts() const;
};

// Largest image side accepted; bigger files are rejected with a clear error
// instead of exhausting memory.
constexpr int MaxImageDimension = 16384;

// Scans an in-memory image. The image is never modified.
ScanResult scanImage(const QImage &image);

// Loads an image file, honouring EXIF orientation and detecting the format
// from the file content. Returns a null image and sets `error` on failure.
QImage loadImage(const QString &path, QString *error = nullptr);

// Loads and scans a file. `loaded` optionally receives the decoded image.
ScanResult scanFile(const QString &path, QImage *loaded = nullptr);

// Lower-case extensions (without dot) the installed Qt image plugins can read.
QStringList supportedImageExtensions();
bool isSupportedImageFile(const QString &path);

} // namespace qrs
