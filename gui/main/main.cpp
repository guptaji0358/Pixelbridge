// Pixelbridge desktop application entry point.
//
//   Pixelbridge.exe                          open the main window
//   Pixelbridge.exe <image>                  same as --scan <image>
//   Pixelbridge.exe --scan <image>           open the Scanner with the image
//   Pixelbridge.exe --generate [text]        open the Generator (optionally pre-filled)
//   Pixelbridge.exe --generate-from-file <f> open the Generator with the text of a file
//   Pixelbridge.exe --copy-path <file>       copy the file's path to the clipboard and exit
//   Pixelbridge.exe --open-default <file>    open the file with its default application and exit
//   Pixelbridge.exe --page <1..5>            start on a given page
//
// The last four --options back the File Explorer right-click entries.

#include "Actions.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QStringDecoder>

static QString readTextFile(const QString &path, QString *error)
{
    constexpr qint64 MaxBytes = 2953; // capacity of the largest QR code
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QObject::tr("Cannot read %1:\n%2").arg(QDir::toNativeSeparators(path), file.errorString());
        return {};
    }
    if (file.size() > MaxBytes) {
        *error = QObject::tr("%1 is %2 bytes; a QR code can hold at most %3 bytes of text.")
                     .arg(QFileInfo(path).fileName()).arg(file.size()).arg(MaxBytes);
        return {};
    }
    const QByteArray bytes = file.readAll();
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8(bytes);
    if (utf8.hasError())
        text = QString::fromLocal8Bit(bytes);
    return text;
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Pixelbridge"));
    QApplication::setApplicationDisplayName(QStringLiteral("Pixelbridge"));
    QApplication::setApplicationVersion(QStringLiteral(QRS_VERSION));
    QApplication::setWindowIcon(app::MainWindow::appIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Pixelbridge - QR scanner and generator"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption scanOpt(QStringLiteral("scan"), QStringLiteral("Scan an image."), QStringLiteral("image"));
    const QCommandLineOption genOpt(QStringLiteral("generate"), QStringLiteral("Open the generator."));
    const QCommandLineOption genFileOpt(QStringLiteral("generate-from-file"),
                                        QStringLiteral("Generate from a text file."), QStringLiteral("file"));
    const QCommandLineOption copyOpt(QStringLiteral("copy-path"), QStringLiteral("Copy a file path."),
                                     QStringLiteral("file"));
    const QCommandLineOption openOpt(QStringLiteral("open-default"),
                                     QStringLiteral("Open a file with its default application."),
                                     QStringLiteral("file"));
    const QCommandLineOption pageOpt(QStringLiteral("page"), QStringLiteral("Start page (1-5)."),
                                     QStringLiteral("number"));
    parser.addOptions({scanOpt, genOpt, genFileOpt, copyOpt, openOpt, pageOpt});
    parser.addPositionalArgument(QStringLiteral("args"), QStringLiteral("Image to scan, or text for --generate."));
    parser.process(application);

    // ---- window-less actions ----------------------------------------------
    if (parser.isSet(copyOpt)) {
        const QString path = QDir::toNativeSeparators(QFileInfo(parser.value(copyOpt)).absoluteFilePath());
        return app::copyTextAndExit(path) ? 0 : 1;
    }
    if (parser.isSet(openOpt)) {
        const QFileInfo file(parser.value(openOpt));
        // Only plain existing files are handed to the shell.
        return file.isFile() && app::openWithDefaultApp(file.absoluteFilePath()) ? 0 : 1;
    }

    app::Settings settings = app::Settings::load();
    app::applyTheme(application, settings.theme);

    app::MainWindow window(&settings);
    const QStringList positional = parser.positionalArguments();

    if (parser.isSet(genFileOpt)) {
        QString error;
        const QString text = readTextFile(parser.value(genFileOpt), &error);
        window.show();
        if (error.isEmpty())
            window.generateFromText(text);
        else
            QMessageBox::warning(&window, QStringLiteral("Pixelbridge"), error);
    } else if (parser.isSet(genOpt)) {
        window.show();
        window.generateFromText(positional.join(u' '));
    } else if (parser.isSet(scanOpt) || !positional.isEmpty()) {
        window.show();
        window.scanFile(parser.isSet(scanOpt) ? parser.value(scanOpt) : positional.first());
    } else {
        if (parser.isSet(pageOpt))
            window.showPage(app::MainWindow::Page(qBound(1, parser.value(pageOpt).toInt(), 5) - 1));
        window.show();
    }
    return QApplication::exec();
}
