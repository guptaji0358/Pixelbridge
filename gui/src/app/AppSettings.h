// Persistent user preferences. Stored as an INI file under the user's
// AppData folder - the application itself never writes to the registry.
#pragma once

#include "qrs/Generator.h"

#include <QString>

namespace app {

enum class ThemeMode { System, Light, Dark };

struct Settings {
    ThemeMode theme = ThemeMode::System;
    int defaultSize = 512;
    int defaultMargin = 4;
    qrs::EccLevel defaultEcc = qrs::EccLevel::Medium;
    QString lastOpenDir;
    QString lastSaveDir;
    QString lastBrowseDir;
    bool confirmOpenLinks = true;

    static Settings load();
    void save() const;
    static QString filePath();
};

} // namespace app
