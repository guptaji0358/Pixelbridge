// FilesPage: actions on the current selection.
#include "FilesPage.h"

#include "Actions.h"

#include <QFileDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>

namespace app {

QStringList FilesPage::selectedPaths() const
{
    QStringList paths;
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    for (const QModelIndex &index : rows)
        paths << m_model->file(index.row()).absoluteFilePath();
    return paths;
}

void FilesPage::updateActions()
{
    const QStringList paths = selectedPaths();
    const int n = int(paths.size());
    int images = 0;
    for (const QString &p : paths)
        images += qrs::isSupportedImageFile(p) ? 1 : 0;

    m_scanButton->setEnabled(n == 1 && images == 1);
    m_batchButton->setEnabled(images >= 1);
    m_openButton->setEnabled(n == 1);
    m_revealButton->setEnabled(n == 1);
    m_copyPathButton->setEnabled(n >= 1);
    m_moveButton->setEnabled(n >= 1);
    m_deleteButton->setEnabled(n >= 1);

    QString text = tr("%n file(s)", nullptr, m_model->rowCount());
    if (n > 0)
        text += tr("  ·  %n selected", nullptr, n);
    if (m_truncated)
        text += tr("  ·  list limited to the first %1 files").arg(m_listing.size());
    m_countLabel->setText(text);
}

// The buttons always act on the selection at click time.
void FilesPage::wireActions()
{
    {
        connect(m_scanButton, &QPushButton::clicked, this, [this] {
            const QStringList s = selectedPaths();
            if (s.size() == 1)
                emit scanRequested(s.first());
        });
        connect(m_batchButton, &QPushButton::clicked, this, [this] {
            QStringList images;
            for (const QString &p : selectedPaths())
                if (qrs::isSupportedImageFile(p))
                    images << p;
            if (!images.isEmpty())
                emit batchRequested(images);
        });
        connect(m_openButton, &QPushButton::clicked, this, [this] {
            const QStringList s = selectedPaths();
            if (s.size() == 1 && !openWithDefaultApp(s.first()))
                emit statusMessage(tr("No application is associated with this file type"));
        });
        connect(m_revealButton, &QPushButton::clicked, this, [this] {
            const QStringList s = selectedPaths();
            if (s.size() == 1)
                revealInExplorer(s.first());
        });
        connect(m_copyPathButton, &QPushButton::clicked, this, [this] {
            QStringList native;
            for (const QString &p : selectedPaths())
                native << QDir::toNativeSeparators(p);
            if (!native.isEmpty()) {
                copyText(native.join(QStringLiteral("\r\n")));
                emit statusMessage(tr("%n path(s) copied to clipboard", nullptr, int(native.size())));
            }
        });
        connect(m_moveButton, &QPushButton::clicked, this, &FilesPage::moveSelected);
        connect(m_deleteButton, &QPushButton::clicked, this, &FilesPage::deleteSelected);
    }
}

void FilesPage::showContextMenu(const QPoint &pos)
{
    if (selectedPaths().isEmpty())
        return;
    QMenu menu(this);
    auto add = [&menu](QPushButton *button, const QString &text) {
        QAction *action = menu.addAction(text, button, &QPushButton::click);
        action->setEnabled(button->isEnabled());
    };
    add(m_scanButton, tr("Open with QR Scanner"));
    add(m_batchButton, tr("Batch scan selected"));
    menu.addSeparator();
    add(m_openButton, tr("Open with default application"));
    add(m_revealButton, tr("Open containing folder"));
    add(m_copyPathButton, tr("Copy file path"));
    menu.addSeparator();
    add(m_moveButton, tr("Move to…"));
    add(m_deleteButton, tr("Delete…"));
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

static QString fileListPreview(const QStringList &paths)
{
    QStringList names;
    for (int i = 0; i < paths.size() && i < 8; ++i)
        names << QStringLiteral("•  ") + QFileInfo(paths[i]).fileName();
    if (paths.size() > 8)
        names << QObject::tr("…and %1 more").arg(paths.size() - 8);
    return names.join(u'\n');
}

static void reportFailures(QWidget *parent, const QString &title, const qrs::FileOpReport &report)
{
    if (report.ok())
        return;
    QMessageBox box(QMessageBox::Warning, title,
                    QObject::tr("%1 file(s) could not be processed.").arg(report.failed.size()),
                    QMessageBox::Ok, parent);
    box.setDetailedText(report.failed.join(u'\n'));
    box.exec();
}

void FilesPage::deleteSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Delete files"));
    box.setText(tr("Move %n file(s) to the Recycle Bin?", nullptr, int(paths.size())));
    box.setInformativeText(fileListPreview(paths)
                           + tr("\n\nYou can restore them from the Recycle Bin. Nothing is deleted permanently."));
    QPushButton *confirm = box.addButton(tr("Move to Recycle Bin"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != confirm)
        return;

    const qrs::FileOpReport report = qrs::moveToTrash(paths);
    emit statusMessage(tr("%n file(s) moved to the Recycle Bin", nullptr, int(report.succeeded.size())));
    reportFailures(this, tr("Delete files"), report);
    reload();
}

void FilesPage::moveSelected()
{
    const QStringList paths = selectedPaths();
    if (paths.isEmpty())
        return;
    const QString target = QFileDialog::getExistingDirectory(this, tr("Move %n file(s) to…", nullptr, int(paths.size())),
                                                             m_folder);
    if (target.isEmpty())
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Move files"));
    box.setText(tr("Move %n file(s) to this folder?", nullptr, int(paths.size())));
    box.setInformativeText(QDir::toNativeSeparators(target) + QStringLiteral("\n\n") + fileListPreview(paths)
                           + tr("\n\nExisting files are never overwritten; a number is added to the name instead."));
    QPushButton *confirm = box.addButton(tr("Move"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() != confirm)
        return;

    const qrs::FileOpReport report = qrs::moveFiles(paths, target);
    emit statusMessage(tr("%n file(s) moved", nullptr, int(report.succeeded.size())));
    reportFailures(this, tr("Move files"), report);
    reload();
}

} // namespace app
