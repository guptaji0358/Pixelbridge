#pragma once

#include "AppSettings.h"
#include "qrs/Scanner.h"

#include <QFutureWatcher>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace app {

class BatchPage : public QWidget {
    Q_OBJECT
public:
    explicit BatchPage(Settings *settings, QWidget *parent = nullptr);

    // Scans an explicit list of files (from the File Explorer page).
    void scanFiles(const QStringList &paths);
    bool isRunning() const { return m_watcher.isRunning(); }
    void cancel();

signals:
    void scanRequested(const QString &path);
    void statusMessage(const QString &message);

private:
    void chooseFolder();
    void startFolder();
    void start(const QStringList &paths);
    void onResult(int index);
    void onFinished();
    void exportCsv();
    void copyAll();
    void setRunning(bool running);
    void updateSummary();
    void showContextMenu(const QPoint &pos);

    Settings *m_settings;
    QLineEdit *m_folderEdit;
    QCheckBox *m_recursive;
    QPushButton *m_startButton, *m_cancelButton, *m_exportButton, *m_copyButton;
    QProgressBar *m_progress;
    QLabel *m_progressLabel, *m_summary, *m_emptyLabel;
    QTableWidget *m_table;

    QFutureWatcher<qrs::ScanResult> m_watcher;
    QVector<qrs::ScanResult> m_results;
    int m_total = 0, m_found = 0, m_none = 0, m_errors = 0, m_codes = 0;
};

} // namespace app
