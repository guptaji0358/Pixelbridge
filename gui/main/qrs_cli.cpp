// qrs_cli - headless front end to the QR Studio core.
// Used by the automated tests, by batch scripts, and for benchmarking.
//
//   qrs_cli encode (--text T | --text-file F) --out FILE [--size N] [--margin N]
//                  [--fg #RRGGBB] [--bg #RRGGBB] [--ecc L|M|Q|H]
//   qrs_cli decode FILE...                 one JSON object per line
//   qrs_cli batch DIR [--recursive] [--csv OUT.csv]
//   qrs_cli formats                        readable image extensions
//
// Exit codes: 0 success, 1 no QR found / operation failed, 2 usage error.

#include "qrs/Files.h"
#include "qrs/Generator.h"
#include "qrs/Scanner.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cstdio>

using namespace qrs;

static int usage()
{
    QTextStream(stderr) << "usage: qrs_cli encode|decode|batch|formats ... (see gui/main/qrs_cli.cpp)\n";
    return 2;
}

static void printLine(const QByteArray &utf8)
{
    std::fwrite(utf8.constData(), 1, size_t(utf8.size()), stdout);
    std::fputc('\n', stdout);
}

static QJsonObject toJson(const ScanResult &r)
{
    QJsonObject o;
    o["file"] = r.filePath;
    o["status"] = r.status == ScanStatus::Found ? "found" : r.status == ScanStatus::NoQr ? "no_qr" : "error";
    o["width"] = r.imageSize.width();
    o["height"] = r.imageSize.height();
    o["elapsed_ms"] = double(r.elapsedMs);
    o["strategies"] = QJsonArray::fromStringList(r.strategiesTried);
    if (!r.error.isEmpty())
        o["error"] = r.error;
    QJsonArray codes;
    for (const QrCode &c : r.codes) {
        QJsonObject j;
        j["decoded"] = c.decoded;
        j["strategy"] = c.strategy;
        QJsonArray corners;
        for (const QPoint &p : c.corners)
            corners.append(QJsonArray{p.x(), p.y()});
        j["corners"] = corners;
        if (c.decoded) {
            j["text"] = c.text;
            j["hex"] = QString::fromLatin1(c.raw.toHex());
            j["version"] = c.version;
            j["ecc"] = c.eccLevel;
            j["mirrored"] = c.mirrored;
        } else {
            j["failure"] = c.failure;
        }
        codes.append(j);
    }
    o["codes"] = codes;
    return o;
}

static int cmdEncode(const QStringList &args)
{
    GenerateOptions opt;
    QString text, out;
    bool haveText = false;
    for (int i = 0; i < args.size(); ++i) {
        const QString a = args[i];
        const bool more = i + 1 < args.size();
        if (a == "--text" && more) { text = args[++i]; haveText = true; }
        else if (a == "--text-file" && more) {
            QFile f(args[++i]);
            if (!f.open(QIODevice::ReadOnly)) {
                QTextStream(stderr) << "cannot read " << f.fileName() << '\n';
                return 1;
            }
            text = QString::fromUtf8(f.readAll());
            haveText = true;
        }
        else if (a == "--out" && more) out = args[++i];
        else if (a == "--size" && more) opt.pixelSize = args[++i].toInt();
        else if (a == "--margin" && more) opt.marginModules = args[++i].toInt();
        else if (a == "--fg" && more) opt.foreground = QColor(args[++i]);
        else if (a == "--bg" && more) opt.background = QColor(args[++i]);
        else if (a == "--ecc" && more) {
            const int idx = QStringLiteral("LMQH").indexOf(args[++i].toUpper());
            if (idx < 0 || args[i].size() != 1)
                return usage();
            opt.ecc = EccLevel(idx);
        }
        else return usage();
    }
    if (!haveText || out.isEmpty())
        return usage();

    const GenerateResult r = generate(text, opt);
    if (!r.ok()) {
        QTextStream(stderr) << "error: " << r.error << '\n';
        return 1;
    }
    if (!r.warning.isEmpty())
        QTextStream(stderr) << "warning: " << r.warning << '\n';

    bool saved;
    if (out.endsWith(".svg", Qt::CaseInsensitive)) {
        QFile f(out);
        saved = f.open(QIODevice::WriteOnly) && f.write(renderSvg(r.matrix, opt)) > 0;
    } else {
        saved = r.image.save(out);
    }
    if (!saved) {
        QTextStream(stderr) << "error: cannot write " << out << '\n';
        return 1;
    }
    QJsonObject o{{"out", out}, {"modules", r.matrix.size}, {"pixels", r.image.width()}};
    printLine(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return 0;
}

static int cmdDecode(const QStringList &files)
{
    if (files.isEmpty())
        return usage();
    bool allFound = true;
    for (const QString &file : files) {
        const ScanResult r = scanFile(file);
        allFound = allFound && r.status == ScanStatus::Found;
        printLine(QJsonDocument(toJson(r)).toJson(QJsonDocument::Compact));
    }
    return allFound ? 0 : 1;
}

static int cmdBatch(const QStringList &args)
{
    QString dir, csv;
    SearchQuery query;
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == "--recursive") query.recursive = true;
        else if (args[i] == "--csv" && i + 1 < args.size()) csv = args[++i];
        else if (dir.isEmpty()) dir = args[i];
        else return usage();
    }
    if (dir.isEmpty())
        return usage();
    if (!QFileInfo(dir).isDir()) {
        QTextStream(stderr) << "error: not a folder: " << dir << '\n';
        return 1;
    }

    QVector<ScanResult> results;
    int found = 0, none = 0, errors = 0;
    for (const QFileInfo &info : findFiles(dir, query)) {
        results << scanFile(info.absoluteFilePath());
        const ScanStatus s = results.last().status;
        (s == ScanStatus::Found ? found : s == ScanStatus::NoQr ? none : errors)++;
    }
    if (!csv.isEmpty()) {
        QString error;
        if (!writeCsv(csv, results, &error)) {
            QTextStream(stderr) << "error: cannot write " << csv << ": " << error << '\n';
            return 1;
        }
    }
    QJsonObject o{{"files", int(results.size())}, {"found", found}, {"no_qr", none}, {"errors", errors}};
    printLine(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return 0;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    if (args.isEmpty())
        return usage();
    const QString cmd = args.takeFirst();
    if (cmd == "encode") return cmdEncode(args);
    if (cmd == "decode") return cmdDecode(args);
    if (cmd == "batch") return cmdBatch(args);
    if (cmd == "formats") {
        printLine(supportedImageExtensions().join(' ').toUtf8());
        return 0;
    }
    return usage();
}
