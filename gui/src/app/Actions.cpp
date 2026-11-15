#include "Actions.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QUrl>

#ifdef Q_OS_WIN
#include <cstring>
#include <string>
#include <windows.h>
#endif

namespace app {

bool openLink(QWidget *parent, const qrs::ContentInfo &info, bool confirm)
{
    if (!info.canOpen())
        return false;
    if (confirm) {
        QMessageBox box(parent);
        box.setIcon(info.warning.isEmpty() ? QMessageBox::Question : QMessageBox::Warning);
        box.setWindowTitle(QObject::tr("Open link?"));
        box.setText(QObject::tr("This will open the following %1 outside Pixelbridge:").arg(info.label.toLower()));
        QString detail = info.openUrl.toDisplayString(QUrl::FullyEncoded);
        if (detail.size() > 400)
            detail = detail.left(400) + QStringLiteral("…");
        QString informative = detail.toHtmlEscaped();
        if (!info.openUrl.host().isEmpty())
            informative += QStringLiteral("<br><br>") + QObject::tr("Destination: <b>%1</b>")
                               .arg(info.openUrl.host(QUrl::FullyEncoded).toHtmlEscaped());
        if (!info.warning.isEmpty())
            informative += QStringLiteral("<br><br>") + info.warning.toHtmlEscaped();
        box.setTextFormat(Qt::RichText); // every dynamic part above is HTML-escaped
        box.setInformativeText(informative);
        QPushButton *open = box.addButton(QObject::tr("Open"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != open)
            return false;
    }
    return QDesktopServices::openUrl(info.openUrl);
}

void revealInExplorer(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (info.isDir()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
        return;
    }
    // Arguments are passed as an array; nothing goes through a shell.
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,"), QDir::toNativeSeparators(info.absoluteFilePath())});
}

bool openWithDefaultApp(const QString &filePath)
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void copyText(const QString &text)
{
    QApplication::clipboard()->setText(text);
}

bool copyTextAndExit(const QString &text)
{
#ifdef Q_OS_WIN
    // Qt's clipboard is OLE-based and its contents vanish when a process exits
    // without having run an event loop (regression seen with --copy-path).
    // Plain Win32 clipboard data is owned by the system once set.
    const std::wstring wide = text.toStdWString();
    const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
    bool ok = false;
    for (int attempt = 0; attempt < 10 && !ok; ++attempt) { // another app may hold the clipboard briefly
        if (!OpenClipboard(nullptr)) {
            Sleep(30);
            continue;
        }
        EmptyClipboard();
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void *target = GlobalLock(memory)) {
                memcpy(target, wide.c_str(), bytes);
                GlobalUnlock(memory);
                ok = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!ok)
                GlobalFree(memory);
        }
        CloseClipboard();
    }
    return ok;
#else
    copyText(text);
    return true;
#endif
}

} // namespace app
