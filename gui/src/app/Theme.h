// Light / dark theming: one palette of design tokens rendered into a style sheet.
#pragma once

#include "AppSettings.h"

#include <QColor>

class QApplication;

namespace app {

struct ThemeColors {
    QColor window, surface, surfaceAlt, border, text, textMuted;
    QColor accent, accentHover, accentText, danger, success, warning, selection;
    bool dark = false;
};

bool systemPrefersDark();
ThemeColors themeColors(ThemeMode mode);

// Applies the Fusion style, palette and application style sheet.
void applyTheme(QApplication &application, ThemeMode mode);

// Colours of the theme currently applied (for custom-painted widgets).
const ThemeColors &currentTheme();

} // namespace app
