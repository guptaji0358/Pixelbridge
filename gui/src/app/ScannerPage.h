#pragma once

#include "AppSettings.h"
#include "qrs/Scanner.h"

#include <QFutureWatcher>
#include <QWidget>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace app {

class ImageView;

class ScannerPage : public QWidget {
    Q_OBJECT
public:
    explicit ScannerPage(Settings *settings, QWidget *parent = nullptr);

public slots:
    void scanFile(const QString &path);
    void chooseFile();
    void pasteImage();
    void clearAll();

signals:
    void statusMessage(const QString &message);

private:
    void startScan(const QImage &image, const QString &path);
    void onScanFinished();
    void showResult(const qrs::ScanResult &result);
    void showCode(int row);
    void setBusy(bool busy);
    void exportResults();
    const qrs::QrCode *currentCode() const;

    Settings *m_settings;
    ImageView *m_view;
    QLabel *m_fileLabel;
    QLabel *m_statusLabel;
    QLabel *m_hintLabel;
    QLabel *m_kindLabel;
    QLabel *m_warningLabel;
    QLabel *m_detailLabel;
    QListWidget *m_codeList;
    QPlainTextEdit *m_text;
    QProgressBar *m_progress;
    QPushButton *m_copyButton;
    QPushButton *m_openButton;
    QPushButton *m_exportButton;
    QPushButton *m_clearButton;

    struct ScanJob {
        qrs::ScanResult result;
        QImage image;
        int generation = 0;
    };
    QFutureWatcher<ScanJob> m_watcher;
    qrs::ScanResult m_result;
    QVector<int> m_decodedIndex; // list row -> index into m_result.codes
    int m_generation = 0;        // discards results of superseded scans
    bool m_pending = false;      // a newer request arrived while a scan was running
    QImage m_pendingImage;
    QString m_pendingPath;
};

} // namespace app
