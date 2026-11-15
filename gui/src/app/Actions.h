// User-triggered shell actions shared by several pages.
#pragma once

#include "qrs/Content.h"

#include <QString>

class QWidget;

namespace app {

// Opens a classified link in the default handler, but only after showing the
// exact destination and getting an explicit confirmation (unless the user
// turned the prompt off in Settings). Returns true if the link was opened.
bool openLink(QWidget *parent, const qrs::ContentInfo &info, bool confirm);

// Opens Explorer with the file selected.
void revealInExplorer(const QString &filePath);

// Opens a file with its associated application.
bool openWithDefaultApp(const QString &filePath);

void copyText(const QString &text);

// For window-less command-line actions: places text on the clipboard so that
// it is still there after this process exits. Returns false on failure.
bool copyTextAndExit(const QString &text);

} // namespace app
