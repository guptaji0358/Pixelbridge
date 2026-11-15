// File-side services: CSV export, filename search/filtering and guarded file
// operations. None of these functions show UI; confirmation is the caller's job.
#pragma once

#include "qrs/Scanner.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>

namespace qrs {

// ---------------------------------------------------------------- CSV ----

// RFC 4180 quoting. Cells starting with = + - @ (or tab / CR) are prefixed
// with an apostrophe so spreadsheets do not execute decoded QR text as a formula.
QString csvEscape(const QString &field);

// One row per decoded symbol; files without a symbol get a single row with a
// status of "no_qr" or "error". Columns:
//   file,folder,status,index,content,type,version,ecc,strategy,error
QString resultsToCsv(const QVector<ScanResult> &results);

// Writes UTF-8 with BOM (so Excel detects the encoding) via an atomic save.
bool writeCsv(const QString &path, const QVector<ScanResult> &results, QString *error = nullptr);

// ------------------------------------------------------------- search ----

struct SearchQuery {
    QString nameContains;     // case-insensitive substring; '*' and '?' wildcards allowed
    QStringList extensions;   // lower-case, no dot; empty = any
    bool imagesOnly = true;   // restrict to supportedImageExtensions()
    bool recursive = false;
};

// Parses a free-form extension filter such as "png, .JPG;*.webp".
QStringList parseExtensions(const QString &text);

// Pure predicate, used for live filtering of an already-listed directory.
bool matches(const QFileInfo &file, const SearchQuery &query);

// Lists matching files below `directory`. `cancel` is polled so a UI can
// abandon a stale search; `maxResults` bounds memory on huge trees.
QVector<QFileInfo> findFiles(const QString &directory, const SearchQuery &query,
                             const std::atomic_bool *cancel = nullptr, int maxResults = 100000);

// ----------------------------------------------------------- file ops ----

struct FileOpReport {
    QStringList succeeded;
    QStringList failed;    // "path: reason"
    bool ok() const { return failed.isEmpty(); }
};

// Sends files to the Recycle Bin. Never deletes permanently: when the
// Recycle Bin is unavailable for a file, that file is reported as failed.
FileOpReport moveToTrash(const QStringList &paths);

// Moves files into `targetDir`. Existing files are never overwritten; a
// numeric suffix " (2)", " (3)", ... is appended instead.
FileOpReport moveFiles(const QStringList &paths, const QString &targetDir);

// Returns a path in `dir` for `fileName` that does not exist yet.
QString uniqueTargetPath(const QString &dir, const QString &fileName);

} // namespace qrs
