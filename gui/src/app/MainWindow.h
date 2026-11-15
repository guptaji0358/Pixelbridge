#pragma once

#include "AppSettings.h"

#include <QMainWindow>

class QListWidget;
class QStackedWidget;

namespace app {

class BatchPage;
class FilesPage;
class GeneratorPage;
class ScannerPage;
class SettingsPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    enum Page { Scanner, Generator, Files, Batch, SettingsTab };

    explicit MainWindow(Settings *settings, QWidget *parent = nullptr);

    void showPage(Page page);
    void scanFile(const QString &path);
    void generateFromText(const QString &text);

    static QIcon appIcon();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Settings *m_settings;
    QListWidget *m_nav;
    QStackedWidget *m_stack;
    ScannerPage *m_scanner;
    GeneratorPage *m_generator;
    FilesPage *m_files;
    BatchPage *m_batch;
    SettingsPage *m_settingsPage;
};

} // namespace app
