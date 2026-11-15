#include "qrs/Files.h"

#include "qrs/Content.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>

namespace qrs {

static QString tr(const char *s) { return QCoreApplication::translate("qrs", s); }

// ---------------------------------------------------------------- CSV ----

QString csvEscape(const QString &field)
{
    QString f = field;
    if (!f.isEmpty()) {
        const QChar c = f[0];
        if (c == u'=' || c == u'+' || c == u'-' || c == u'@' || c == u'\t' || c == u'\r')
            f.prepend(u'\'');
    }
    if (f.contains(u'"') || f.contains(u',') || f.contains(u'\n') || f.contains(u'\r')) {
        f.replace(u'"', QLatin1String("\"\""));
        f = u'"' + f + u'"';
    }
    return f;
}

static QString row(const QStringList &cells)
{
    QStringList escaped;
    for (const QString &c : cells)
        escaped << csvEscape(c);
    return escaped.join(u',') + QLatin1String("\r\n");
}

QString resultsToCsv(const QVector<ScanResult> &results)
{
    static const char *const ecc[] = {"L", "M", "Q", "H"};
    QString out = QStringLiteral("file,folder,status,index,content,type,version,ecc,strategy,error\r\n");
    for (const ScanResult &r : results) {
        const QFileInfo info(r.filePath);
        const QString name = info.fileName();
        const QString folder = QDir::toNativeSeparators(info.absolutePath());
        if (r.status == ScanStatus::LoadError) {
            out += row({name, folder, QStringLiteral("error"), {}, {}, {}, {}, {}, {}, r.error});
            continue;
        }
        if (r.decodedCount() == 0) {
            out += row({name, folder, QStringLiteral("no_qr"), {}, {}, {}, {}, {}, {}, {}});
            continue;
        }
        int index = 0;
        for (const QrCode &c : r.codes) {
            if (!c.decoded)
                continue;
            out += row({name, folder, QStringLiteral("ok"), QString::number(++index), c.text,
                        classify(c.text).label, QString::number(c.version),
                        QString::fromLatin1(ecc[qBound(0, c.eccLevel, 3)]), c.strategy, {}});
        }
    }
    return out;
}

bool writeCsv(const QString &path, const QVector<ScanResult> &results, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write("\xEF\xBB\xBF");
    file.write(resultsToCsv(results).toUtf8());
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

// ------------------------------------------------------------- search ----

QStringList parseExtensions(const QString &text)
{
    QStringList out;
    static const QRegularExpression sep(QStringLiteral(R"([,;\s|]+)"));
    const QStringList parts = text.split(sep, Qt::SkipEmptyParts);
    for (QString p : parts) {
        p = p.toLower();
        while (p.startsWith(u'*') || p.startsWith(u'.'))
            p.remove(0, 1);
        if (!p.isEmpty() && !out.contains(p))
            out << p;
    }
    return out;
}

bool matches(const QFileInfo &file, const SearchQuery &query)
{
    const QString ext = file.suffix().toLower();
    if (query.imagesOnly && !supportedImageExtensions().contains(ext))
        return false;
    if (!query.extensions.isEmpty() && !query.extensions.contains(ext))
        return false;

    const QString needle = query.nameContains.trimmed();
    if (needle.isEmpty())
        return true;
    const QString name = file.fileName();
    if (needle.contains(u'*') || needle.contains(u'?')) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(
                                        needle, QRegularExpression::UnanchoredWildcardConversion),
                                    QRegularExpression::CaseInsensitiveOption);
        return re.match(name).hasMatch();
    }
    return name.contains(needle, Qt::CaseInsensitive);
}

QVector<QFileInfo> findFiles(const QString &directory, const SearchQuery &query,
                             const std::atomic_bool *cancel, int maxResults)
{
    QVector<QFileInfo> out;
    if (directory.isEmpty() || !QFileInfo(directory).isDir())
        return out;
    QDirIterator it(directory, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
                    query.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        if (cancel && cancel->load(std::memory_order_relaxed))
            break;
        const QFileInfo info = it.nextFileInfo();
        if (matches(info, query)) {
            out << info;
            if (out.size() >= maxResults)
                break;
        }
    }
    return out;
}

// ----------------------------------------------------------- file ops ----

FileOpReport moveToTrash(const QStringList &paths)
{
    FileOpReport report;
    for (const QString &path : paths) {
        QFile file(path);
        if (!file.exists()) {
            report.failed << path + QLatin1String(": ") + tr("file not found");
        } else if (file.moveToTrash()) {
            report.succeeded << path;
        } else {
            report.failed << path + QLatin1String(": ")
                                 + (file.errorString().isEmpty() ? tr("could not be moved to the Recycle Bin")
                                                                 : file.errorString());
        }
    }
    return report;
}

QString uniqueTargetPath(const QString &dir, const QString &fileName)
{
    const QDir d(dir);
    QString candidate = d.filePath(fileName);
    if (!QFileInfo::exists(candidate))
        return candidate;
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : u'.' + info.suffix();
    for (int i = 2; i < 100000; ++i) {
        candidate = d.filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return {};
}

FileOpReport moveFiles(const QStringList &paths, const QString &targetDir)
{
    FileOpReport report;
    const QFileInfo target(targetDir);
    if (!target.isDir()) {
        for (const QString &path : paths)
            report.failed << path + QLatin1String(": ") + tr("target folder does not exist");
        return report;
    }
    const QString targetCanonical = target.canonicalFilePath();
    for (const QString &path : paths) {
        const QFileInfo source(path);
        if (!source.isFile()) {
            report.failed << path + QLatin1String(": ") + tr("file not found");
            continue;
        }
        if (source.canonicalPath() == targetCanonical) {
            report.failed << path + QLatin1String(": ") + tr("already in the target folder");
            continue;
        }
        const QString destination = uniqueTargetPath(targetCanonical, source.fileName());
        if (destination.isEmpty()) {
            report.failed << path + QLatin1String(": ") + tr("no free file name in the target folder");
            continue;
        }
        QFile file(path);
        // QFile::rename never overwrites and falls back to copy + remove across volumes.
        if (file.rename(destination))
            report.succeeded << destination;
        else
            report.failed << path + QLatin1String(": ") + file.errorString();
    }
    return report;
}

} // namespace qrs
