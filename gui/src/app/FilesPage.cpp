// FilesPage: construction, background listing and live filtering.
// Selection actions live in FilesPageActions.cpp.
#include "FilesPage.h"

#include "Widgets.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace app {

static constexpr int MaxListing = 200000;

FilesPage::FilesPage(Settings *settings, QWidget *parent) : QWidget(parent), m_settings(settings)
{
    setObjectName("page");
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);
    root->addWidget(makeHeader(tr("File Explorer"),
                               tr("Browse a folder, search by name or type, then scan or manage the selected files.")));

    auto *folderRow = new QHBoxLayout;
    m_folderEdit = new QLineEdit;
    m_folderEdit->setPlaceholderText(tr("Folder path"));
    auto *browse = makeButton(tr("Browse…"));
    auto *refresh = makeButton(tr("Refresh"));
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(browse);
    folderRow->addWidget(refresh);
    root->addLayout(folderRow);

    auto *filterRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search by file name  (wildcards * and ? allowed)"));
    m_search->setClearButtonEnabled(true);
    m_extFilter = new QLineEdit;
    m_extFilter->setPlaceholderText(tr("Extensions, e.g. png, jpg"));
    m_extFilter->setClearButtonEnabled(true);
    m_extFilter->setMaximumWidth(210);
    m_imagesOnly = new QCheckBox(tr("Images only"));
    m_imagesOnly->setChecked(true);
    m_recursive = new QCheckBox(tr("Include subfolders"));
    filterRow->addWidget(m_search, 1);
    filterRow->addWidget(m_extFilter);
    filterRow->addWidget(m_imagesOnly);
    filterRow->addWidget(m_recursive);
    root->addLayout(filterRow);

    m_model = new FileTableModel(this);
    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(FileTableModel::Name, Qt::AscendingOrder);
    m_table->setWordWrap(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(30);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(FileTableModel::Name, QHeaderView::Stretch);
    m_table->setColumnWidth(FileTableModel::Type, 70);
    m_table->setColumnWidth(FileTableModel::Size, 90);
    m_table->setColumnWidth(FileTableModel::Modified, 140);
    m_table->setColumnWidth(FileTableModel::Folder, 240);
    root->addWidget(m_table, 1);

    m_emptyLabel = makeLabel(QString(), "muted");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setMinimumHeight(120);
    root->addWidget(m_emptyLabel, 1);

    auto *actions = new QHBoxLayout;
    m_countLabel = makeLabel(QString(), "muted");
    m_scanButton = makeButton(tr("Scan"), "primary");
    m_scanButton->setToolTip(tr("Open the selected image in the QR Scanner"));
    m_batchButton = makeButton(tr("Batch scan selected"));
    m_openButton = makeButton(tr("Open"));
    m_openButton->setToolTip(tr("Open with the default application"));
    m_revealButton = makeButton(tr("Show in folder"));
    m_copyPathButton = makeButton(tr("Copy path"));
    m_moveButton = makeButton(tr("Move…"));
    m_deleteButton = makeButton(tr("Delete…"), "danger");
    m_deleteButton->setToolTip(tr("Move the selected files to the Recycle Bin (asks first)"));
    actions->addWidget(m_countLabel, 1);
    for (QPushButton *b : {m_scanButton, m_batchButton, m_openButton, m_revealButton, m_copyPathButton,
                           m_moveButton, m_deleteButton})
        actions->addWidget(b);
    root->addLayout(actions);

    m_filterDebounce.setSingleShot(true);
    m_filterDebounce.setInterval(120);
    connect(&m_filterDebounce, &QTimer::timeout, this, &FilesPage::applyFilter);
    auto filterLater = [this] { m_filterDebounce.start(); };
    connect(m_search, &QLineEdit::textChanged, this, filterLater);
    connect(m_extFilter, &QLineEdit::textChanged, this, filterLater);
    connect(m_imagesOnly, &QCheckBox::toggled, this, filterLater);
    connect(m_recursive, &QCheckBox::toggled, this, &FilesPage::reload);
    connect(browse, &QPushButton::clicked, this, &FilesPage::chooseFolder);
    connect(refresh, &QPushButton::clicked, this, &FilesPage::reload);
    connect(m_folderEdit, &QLineEdit::returnPressed, this, [this] { setFolder(m_folderEdit->text().trimmed()); });
    connect(&m_watcher, &QFutureWatcher<QVector<QFileInfo>>::finished, this, &FilesPage::onListed);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FilesPage::updateActions);
    connect(m_model, &QAbstractItemModel::modelReset, this, &FilesPage::updateActions);
    connect(m_table, &QTableView::customContextMenuRequested, this, &FilesPage::showContextMenu);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        const QFileInfo &file = m_model->file(index.row());
        if (qrs::isSupportedImageFile(file.filePath()))
            emit scanRequested(file.absoluteFilePath());
    });

    wireActions();
    updateActions();

    QString start = m_settings->lastBrowseDir;
    if (start.isEmpty() || !QFileInfo(start).isDir())
        start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    setFolder(start);
}

void FilesPage::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a folder"), m_folder);
    if (!dir.isEmpty())
        setFolder(dir);
}

void FilesPage::setFolder(const QString &folder)
{
    m_folder = QDir::cleanPath(folder);
    m_folderEdit->setText(QDir::toNativeSeparators(m_folder));
    if (QFileInfo(m_folder).isDir())
        m_settings->lastBrowseDir = m_folder;
    reload();
}

void FilesPage::reload()
{
    if (m_cancel)
        m_cancel->store(true); // abandon a listing that is still running
    if (m_watcher.isRunning()) {
        m_reloadPending = true;
        return;
    }
    m_listing.clear();
    if (!QFileInfo(m_folder).isDir()) {
        m_model->setFiles({});
        m_table->setVisible(false);
        m_emptyLabel->setVisible(true);
        m_emptyLabel->setText(m_folder.isEmpty() ? tr("Choose a folder to begin.")
                                                 : tr("This folder does not exist or cannot be opened."));
        m_countLabel->clear();
        return;
    }
    m_countLabel->setText(tr("Listing files…"));
    m_cancel = std::make_shared<std::atomic_bool>(false);
    qrs::SearchQuery everything;
    everything.imagesOnly = false;
    everything.recursive = m_recursive->isChecked();
    const QString folder = m_folder;
    const auto cancel = m_cancel;
    m_watcher.setFuture(QtConcurrent::run([folder, everything, cancel] {
        return qrs::findFiles(folder, everything, cancel.get(), MaxListing);
    }));
}

void FilesPage::onListed()
{
    if (m_reloadPending) {
        m_reloadPending = false;
        reload();
        return;
    }
    m_listing = m_watcher.result();
    m_truncated = m_listing.size() >= MaxListing;
    applyFilter();
}

void FilesPage::applyFilter()
{
    qrs::SearchQuery query;
    query.nameContains = m_search->text();
    query.extensions = qrs::parseExtensions(m_extFilter->text());
    query.imagesOnly = m_imagesOnly->isChecked();

    QVector<QFileInfo> shown;
    shown.reserve(m_listing.size());
    for (const QFileInfo &file : m_listing)
        if (qrs::matches(file, query))
            shown << file;
    m_model->setFiles(shown);

    const bool empty = shown.isEmpty();
    m_table->setVisible(!empty);
    m_emptyLabel->setVisible(empty);
    if (empty) {
        const bool filtered = !query.nameContains.trimmed().isEmpty() || !query.extensions.isEmpty();
        m_emptyLabel->setText(m_listing.isEmpty()
                                  ? tr("This folder is empty.")
                                  : filtered ? tr("No files match your search.\nClear the search box or the extension filter.")
                                             : tr("No images in this folder.\nUntick “Images only” to see every file."));
    }
    updateActions();
}

} // namespace app
