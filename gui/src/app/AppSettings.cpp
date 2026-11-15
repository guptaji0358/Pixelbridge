#include "AppSettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace app {

QString Settings::filePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(dir).filePath(QStringLiteral("settings.ini"));
}

Settings Settings::load()
{
    Settings s;
    const QSettings ini(filePath(), QSettings::IniFormat);
    s.theme = ThemeMode(qBound(0, ini.value("ui/theme", 0).toInt(), 2));
    s.defaultSize = qBound(qrs::limits::MinPixelSize, ini.value("generator/size", 512).toInt(),
                           qrs::limits::MaxPixelSize);
    s.defaultMargin = qBound(0, ini.value("generator/margin", 4).toInt(), qrs::limits::MaxMargin);
    s.defaultEcc = qrs::EccLevel(qBound(0, ini.value("generator/ecc", 1).toInt(), 3));
    s.lastOpenDir = ini.value("paths/open").toString();
    s.lastSaveDir = ini.value("paths/save").toString();
    s.lastBrowseDir = ini.value("paths/browse").toString();
    s.confirmOpenLinks = ini.value("safety/confirmOpenLinks", true).toBool();
    return s;
}

void Settings::save() const
{
    QDir().mkpath(QFileInfo(filePath()).absolutePath());
    QSettings ini(filePath(), QSettings::IniFormat);
    ini.setValue("ui/theme", int(theme));
    ini.setValue("generator/size", defaultSize);
    ini.setValue("generator/margin", defaultMargin);
    ini.setValue("generator/ecc", int(defaultEcc));
    ini.setValue("paths/open", lastOpenDir);
    ini.setValue("paths/save", lastSaveDir);
    ini.setValue("paths/browse", lastBrowseDir);
    ini.setValue("safety/confirmOpenLinks", confirmOpenLinks);
}

} // namespace app
