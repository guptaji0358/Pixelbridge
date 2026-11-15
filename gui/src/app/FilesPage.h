#pragma once

#include "AppSettings.h"
#include "qrs/Files.h"

#include <QAbstractTableModel>
#include <QFutureWatcher>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <memory>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace app {

// Flat, sortable list of files. Holds only QFileInfo values, so tens of
// thousands of rows stay cheap.
class FileTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Name, Type, Size, Modified, Folder, ColumnCount };
    using QAbstractTableModel::QAbstractTableModel;

    void setFiles(const QVector<QFileInfo> &files);
    const QFileInfo &file(int row) const { return m_files[row]; }

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void sort(int column, Qt::SortOrder order) override;

private:
    QVector<QFileInfo> m_files;
    int m_sortColumn = Name;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

class FilesPage : public QWidget {
    Q_OBJECT
public:
    explicit FilesPage(Settings *settings, QWidget *parent = nullptr);
    void setFolder(const QString &folder);

signals:
    void scanRequested(const QString &path);
    void batchRequested(const QStringList &paths);
    void statusMessage(const QString &message);

private:
    void chooseFolder();
    void reload();          // re-lists the folder in the background
    void onListed();
    void applyFilter();     // live, in-memory filtering of the listing
    void updateActions();
    void wireActions();
    QStringList selectedPaths() const;
    void showContextMenu(const QPoint &pos);
    void deleteSelected();
    void moveSelected();

    Settings *m_settings;
    QLineEdit *m_folderEdit;
    QLineEdit *m_search;
    QLineEdit *m_extFilter;
    QCheckBox *m_imagesOnly;
    QCheckBox *m_recursive;
    QTableView *m_table;
    FileTableModel *m_model;
    QLabel *m_countLabel;
    QLabel *m_emptyLabel;
    QPushButton *m_scanButton, *m_batchButton, *m_openButton, *m_revealButton;
    QPushButton *m_copyPathButton, *m_moveButton, *m_deleteButton;

    QString m_folder;
    QVector<QFileInfo> m_listing; // everything in the folder, unfiltered
    QTimer m_filterDebounce;
    QFutureWatcher<QVector<QFileInfo>> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_reloadPending = false;
    bool m_truncated = false;
};

} // namespace app
