#include "FilesPage.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QLocale>

#include <algorithm>

namespace app {

void FileTableModel::setFiles(const QVector<QFileInfo> &files)
{
    beginResetModel();
    m_files = files;
    endResetModel();
    sort(m_sortColumn, m_sortOrder);
}

int FileTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_files.size());
}

int FileTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(ColumnCount);
}

QVariant FileTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_files.size())
        return {};
    const QFileInfo &f = m_files[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Name:     return f.fileName();
        case Type:     return f.suffix().toUpper();
        case Size:     return QLocale().formattedDataSize(f.size(), 1, QLocale::DataSizeTraditionalFormat);
        case Modified: return QLocale().toString(f.lastModified(), QLocale::ShortFormat);
        case Folder:   return QDir::toNativeSeparators(f.absolutePath());
        }
    } else if (role == Qt::ToolTipRole) {
        return QDir::toNativeSeparators(f.absoluteFilePath());
    } else if (role == Qt::TextAlignmentRole && index.column() == Size) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    return {};
}

QVariant FileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case Name:     return tr("Name");
    case Type:     return tr("Type");
    case Size:     return tr("Size");
    case Modified: return tr("Modified");
    case Folder:   return tr("Folder");
    }
    return {};
}

void FileTableModel::sort(int column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;
    QCollator collator;
    collator.setNumericMode(true); // "img2" before "img10"
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    auto less = [&](const QFileInfo &a, const QFileInfo &b) {
        switch (column) {
        case Type: {
            const int c = a.suffix().compare(b.suffix(), Qt::CaseInsensitive);
            return c != 0 ? c < 0 : collator.compare(a.fileName(), b.fileName()) < 0;
        }
        case Size:     return a.size() < b.size();
        case Modified: return a.lastModified() < b.lastModified();
        case Folder: {
            const int c = collator.compare(a.absolutePath(), b.absolutePath());
            return c != 0 ? c < 0 : collator.compare(a.fileName(), b.fileName()) < 0;
        }
        default:       return collator.compare(a.fileName(), b.fileName()) < 0;
        }
    };

    // A full reset (rather than layoutChanged) deliberately drops the selection,
    // so a destructive action can never apply to rows that moved under it.
    beginResetModel();
    if (order == Qt::AscendingOrder)
        std::stable_sort(m_files.begin(), m_files.end(), less);
    else
        std::stable_sort(m_files.begin(), m_files.end(),
                         [&](const QFileInfo &a, const QFileInfo &b) { return less(b, a); });
    endResetModel();
}

} // namespace app
