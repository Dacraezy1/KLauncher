#include "ModsPage.h"
#include "../mods/ModSearch.h"
#include "../minecraft/InstanceManager.h"
#include "../utils/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QTabWidget>
#include <QMessageBox>

ModsPage::ModsPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    populateInstances();
    connect(&InstanceManager::instance(), &InstanceManager::instancesChanged,
            this, &ModsPage::populateInstances);
}

void ModsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);

    // Page title
    auto* hdr = new QHBoxLayout;
    auto* title = new QLabel("Mod Browser");
    title->setObjectName("pageTitle");
    hdr->addWidget(title);
    hdr->addStretch();
    root->addLayout(hdr);

    // Top search bar
    auto* searchBar = new QHBoxLayout;
    searchBar->setSpacing(8);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("🔍  Search mods...");
    m_searchEdit->setFixedHeight(36);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &ModsPage::onSearch);
    searchBar->addWidget(m_searchEdit, 2);

    m_sourceBox = new QComboBox;
    m_sourceBox->addItem("Modrinth", "modrinth");
    m_sourceBox->addItem("CurseForge", "curseforge");
    m_sourceBox->setFixedHeight(36);
    m_sourceBox->setMinimumWidth(120);
    connect(m_sourceBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModsPage::onSourceChanged);
    searchBar->addWidget(m_sourceBox);

    m_loaderFilter = new QComboBox;
    m_loaderFilter->addItem("All Loaders", "");
    m_loaderFilter->addItem("Fabric",  "fabric");
    m_loaderFilter->addItem("Forge",   "forge");
    m_loaderFilter->addItem("NeoForge","neoforge");
    m_loaderFilter->addItem("Quilt",   "quilt");
    m_loaderFilter->setFixedHeight(36);
    m_loaderFilter->setMinimumWidth(120);
    searchBar->addWidget(m_loaderFilter);

    m_searchBtn = new QPushButton("Search");
    m_searchBtn->setFixedHeight(36);
    m_searchBtn->setFixedWidth(80);
    connect(m_searchBtn, &QPushButton::clicked, this, &ModsPage::onSearch);
    searchBar->addWidget(m_searchBtn);
    root->addLayout(searchBar);

    // Instance selector
    auto* instRow = new QHBoxLayout;
    instRow->addWidget(new QLabel("Install to:"));
    m_instanceBox = new QComboBox;
    m_instanceBox->setFixedHeight(32);
    m_instanceBox->setMinimumWidth(240);
    instRow->addWidget(m_instanceBox);
    instRow->addStretch();
    root->addLayout(instRow);

    // Main splitter: results list | detail panel
    auto* splitter = new QSplitter(Qt::Horizontal);

    // ── Left: tabs (search results / installed)
    auto* leftTabs = new QTabWidget;

    // Search results tab
    auto* searchTab = new QWidget;
    auto* searchTabLayout = new QVBoxLayout(searchTab);
    searchTabLayout->setContentsMargins(0, 4, 0, 0);
    searchTabLayout->setSpacing(6);

    m_resultCount = new QLabel("Enter a search query above");
    m_resultCount->setStyleSheet("color: #90caf9; font-size: 12px; padding: 0 4px;");
    searchTabLayout->addWidget(m_resultCount);

    m_resultsList = new QListWidget;
    m_resultsList->setSpacing(2);
    m_resultsList->setIconSize(QSize(40, 40));
    connect(m_resultsList, &QListWidget::currentRowChanged,
            this, &ModsPage::onResultSelected);
    searchTabLayout->addWidget(m_resultsList, 1);

    // Pagination
    auto* pageRow = new QHBoxLayout;
    m_prevBtn = new QPushButton("◀ Prev");
    m_prevBtn->setFixedHeight(28);
    m_prevBtn->setEnabled(false);
    connect(m_prevBtn, &QPushButton::clicked, this, &ModsPage::onPrevPage);
    pageRow->addWidget(m_prevBtn);

    m_pageLabel = new QLabel("Page 1");
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setStyleSheet("color: #90caf9; font-size: 12px;");
    pageRow->addWidget(m_pageLabel, 1);

    m_nextBtn = new QPushButton("Next ▶");
    m_nextBtn->setFixedHeight(28);
    m_nextBtn->setEnabled(false);
    connect(m_nextBtn, &QPushButton::clicked, this, &ModsPage::onNextPage);
    pageRow->addWidget(m_nextBtn);
    searchTabLayout->addLayout(pageRow);

    leftTabs->addTab(searchTab, "🔍 Search");

    // Installed mods tab
    auto* installedTab = new QWidget;
    auto* instTabLayout = new QVBoxLayout(installedTab);
    instTabLayout->setContentsMargins(0, 4, 0, 0);
    instTabLayout->setSpacing(6);

    m_installedList = new QListWidget;
    m_installedList->setSpacing(2);
    connect(m_installedList, &QListWidget::currentRowChanged, this, [this](int row){
        m_removeModBtn->setEnabled(row >= 0);
    });
    instTabLayout->addWidget(m_installedList, 1);

    auto* instBtnRow = new QHBoxLayout;
    m_removeModBtn = new QPushButton("🗑  Remove Mod");
    m_removeModBtn->setObjectName("dangerBtn");
    m_removeModBtn->setFixedHeight(32);
    m_removeModBtn->setEnabled(false);
    connect(m_removeModBtn, &QPushButton::clicked, this, [this](){
        auto* item = m_installedList->currentItem();
        if (!item) return;
        QString filePath = item->data(Qt::UserRole).toString();
        if (QMessageBox::question(this, "Remove Mod",
            "Remove " + item->text() + "?",
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            QFile::remove(filePath);
            refreshInstalledMods();
        }
    });
    instBtnRow->addWidget(m_removeModBtn);

    m_openFolderBtn = new QPushButton("📁  Open Mods Folder");
    m_openFolderBtn->setFixedHeight(32);
    connect(m_openFolderBtn, &QPushButton::clicked, this, [this](){
        int idx = m_instanceBox->currentIndex();
        if (idx < 0) return;
        QString instId = m_instanceBox->itemData(idx).toString();
        auto inst = InstanceManager::instance().findById(instId);
        if (!inst.id.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(inst.modsDir()));
    });
    instBtnRow->addWidget(m_openFolderBtn);
    instBtnRow->addStretch();
    instTabLayout->addLayout(instBtnRow);

    leftTabs->addTab(installedTab, "📦 Installed");
    connect(leftTabs, &QTabWidget::currentChanged, this, [this](int tab){
        if (tab == 1) refreshInstalledMods();
    });

    splitter->addWidget(leftTabs);

    // ── Right: detail panel
    auto* detailScroll = new QScrollArea;
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* detailW = new QWidget;
    auto* detailLayout = new QVBoxLayout(detailW);
    detailLayout->setContentsMargins(16, 12, 16, 12);
    detailLayout->setSpacing(12);

    // Icon + title
    auto* modHeaderRow = new QHBoxLayout;
    m_modIcon = new QLabel;
    m_modIcon->setFixedSize(64, 64);
    m_modIcon->setAlignment(Qt::AlignCenter);
    m_modIcon->setText("🧩");
    m_modIcon->setStyleSheet(
        "font-size:36px; background:rgba(15,52,96,0.5); border-radius:10px; border:1px solid #2d4a7a;");
    modHeaderRow->addWidget(m_modIcon);

    auto* titleCol = new QVBoxLayout;
    m_modTitle = new QLabel("Select a mod");
    m_modTitle->setStyleSheet("font-size:17px; font-weight:700; color:#e0e0e0;");
    m_modTitle->setWordWrap(true);
    titleCol->addWidget(m_modTitle);

    m_modMeta = new QLabel;
    m_modMeta->setStyleSheet("color:#90caf9; font-size:12px;");
    m_modMeta->setWordWrap(true);
    titleCol->addWidget(m_modMeta);
    modHeaderRow->addLayout(titleCol, 1);
    detailLayout->addLayout(modHeaderRow);

    m_modDesc = new QLabel;
    m_modDesc->setWordWrap(true);
    m_modDesc->setStyleSheet("color:#c0c8d8; font-size:13px; line-height:1.5;");
    m_modDesc->setTextFormat(Qt::PlainText);
    detailLayout->addWidget(m_modDesc);

    // Version selector + download
    auto* versionGroup = new QGroupBox("Version");
    auto* versionLayout = new QVBoxLayout(versionGroup);

    m_versionBox = new QComboBox;
    m_versionBox->setFixedHeight(34);
    m_versionBox->setEnabled(false);
    versionLayout->addWidget(m_versionBox);

    m_downloadBtn = new QPushButton("⬇  Install Mod");
    m_downloadBtn->setObjectName("launchBtn");
    m_downloadBtn->setFixedHeight(38);
    m_downloadBtn->setEnabled(false);
    connect(m_downloadBtn, &QPushButton::clicked, this, &ModsPage::onDownload);
    versionLayout->addWidget(m_downloadBtn);

    m_dlProgress = new QProgressBar;
    m_dlProgress->setFixedHeight(8);
    m_dlProgress->setVisible(false);
    versionLayout->addWidget(m_dlProgress);

    m_dlStatus = new QLabel;
    m_dlStatus->setStyleSheet("color:#69f0ae; font-size:12px;");
    versionLayout->addWidget(m_dlStatus);

    detailLayout->addWidget(versionGroup);
    detailLayout->addStretch();

    detailScroll->setWidget(detailW);
    splitter->addWidget(detailScroll);
    splitter->setSizes({420, 380});

    root->addWidget(splitter, 1);
}

void ModsPage::populateInstances() {
    m_instanceBox->clear();
    for (auto& inst : InstanceManager::instance().instances()) {
        QString label = inst.name + " (" + inst.mcVersion + ")";
        m_instanceBox->addItem(label, inst.id);
    }
    if (m_instanceBox->count() == 0)
        m_instanceBox->addItem("No instances — create one first", "");
}

void ModsPage::onSourceChanged(int) {
    m_resultsList->clear();
    m_results.clear();
    m_resultCount->setText("Enter a search query above");
    m_modTitle->setText("Select a mod");
    m_modDesc->clear();
    m_modMeta->clear();
    m_versionBox->clear();
    m_versionBox->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_dlStatus->clear();
}

void ModsPage::onSearch() {
    m_currentPage = 0;
    QString query  = m_searchEdit->text().trimmed();
    QString source = m_sourceBox->currentData().toString();
    QString loader = m_loaderFilter->currentData().toString();

    // Try to auto-detect MC version from selected instance
    QString mcVer;
    int instIdx = m_instanceBox->currentIndex();
    if (instIdx >= 0) {
        QString instId = m_instanceBox->itemData(instIdx).toString();
        auto inst = InstanceManager::instance().findById(instId);
        if (!inst.id.isEmpty()) mcVer = inst.mcVersion;
    }

    m_searchBtn->setEnabled(false);
    m_resultsList->clear();
    m_resultCount->setText("Searching...");

    auto cb = [this](bool ok, QList<ModResult> results, int total){
        QMetaObject::invokeMethod(this, [this, ok, results, total](){
            m_searchBtn->setEnabled(true);
            if (!ok) { m_resultCount->setText("Search failed"); return; }
            m_results   = results;
            m_totalResults = total;
            displayResults(results, total);
        }, Qt::QueuedConnection);
    };

    if (source == "modrinth")
        ModSearch::instance().searchModrinth(query, mcVer, loader,
            m_currentPage * PAGE_SIZE, PAGE_SIZE, cb);
    else
        ModSearch::instance().searchCurseForge(query, mcVer, loader,
            m_currentPage * PAGE_SIZE, PAGE_SIZE, cb);
}

void ModsPage::displayResults(const QList<ModResult>& results, int total) {
    m_resultsList->clear();
    int pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    m_resultCount->setText(QString("%1 results  (page %2/%3)")
        .arg(total).arg(m_currentPage + 1).arg(qMax(pages,1)));
    m_prevBtn->setEnabled(m_currentPage > 0);
    m_nextBtn->setEnabled((m_currentPage + 1) * PAGE_SIZE < total);
    m_pageLabel->setText(QString("Page %1 / %2").arg(m_currentPage+1).arg(qMax(pages,1)));

    for (auto& r : results) {
        auto* item = new QListWidgetItem;
        item->setText(r.title + "\n" + r.author + "  •  " +
                      QString::number(r.downloads) + " downloads");
        item->setData(Qt::UserRole, m_results.indexOf(r));
        m_resultsList->addItem(item);

        // Async icon load
        if (!r.iconUrl.isEmpty()) {
            int row = m_resultsList->count() - 1;
            ModSearch::instance().fetchIcon(r.iconUrl, [this, row](QPixmap px){
                QMetaObject::invokeMethod(this, [this, row, px](){
                    if (row < m_resultsList->count() && !px.isNull())
                        m_resultsList->item(row)->setIcon(
                            QIcon(px.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation)));
                }, Qt::QueuedConnection);
            });
        }
    }

    if (results.isEmpty())
        m_resultsList->addItem("No results found");
}

void ModsPage::onResultSelected(int row) {
    if (row < 0 || row >= m_results.size()) return;
    const auto& mod = m_results[row];

    m_modTitle->setText(mod.title);
    m_modDesc->setText(mod.description);
    m_modMeta->setText(
        "By " + mod.author + "  •  " +
        QString::number(mod.downloads) + " downloads  •  " +
        mod.source.toUpper());

    m_versionBox->clear();
    m_versionBox->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    m_dlStatus->setText("Loading versions...");
    m_modVersions.clear();

    // Detect loader from selected instance
    QString loader, mcVer;
    int instIdx = m_instanceBox->currentIndex();
    if (instIdx >= 0) {
        auto inst = InstanceManager::instance().findById(
            m_instanceBox->itemData(instIdx).toString());
        if (!inst.id.isEmpty()) {
            mcVer = inst.mcVersion;
            switch (inst.modLoader) {
                case ModLoaderType::Fabric:   loader = "fabric";   break;
                case ModLoaderType::Forge:    loader = "forge";    break;
                case ModLoaderType::NeoForge: loader = "neoforge"; break;
                case ModLoaderType::Quilt:    loader = "quilt";    break;
                default: break;
            }
        }
    }

    ModSearch::instance().getModrinthVersions(mod.id, mcVer, loader,
        [this](bool ok, QList<ModVersion> versions){
            QMetaObject::invokeMethod(this, [this, ok, versions](){
                if (!ok || versions.isEmpty()) {
                    m_dlStatus->setText("No compatible versions found");
                    return;
                }
                m_modVersions = versions;
                m_versionBox->clear();
                for (auto& v : versions) {
                    QString label = v.versionNumber + "  [" + v.mcVersion + "]";
                    if (!v.loaderType.isEmpty()) label += "  " + v.loaderType;
                    m_versionBox->addItem(label, v.id);
                }
                m_versionBox->setEnabled(true);
                m_downloadBtn->setEnabled(true);
                m_dlStatus->setText(QString("%1 versions available").arg(versions.size()));
            }, Qt::QueuedConnection);
        });
}

void ModsPage::onDownload() {
    int instIdx = m_instanceBox->currentIndex();
    if (instIdx < 0) return;
    QString instId = m_instanceBox->itemData(instIdx).toString();
    if (instId.isEmpty()) {
        m_dlStatus->setText("Please select an instance first");
        return;
    }
    auto inst = InstanceManager::instance().findById(instId);
    if (inst.id.isEmpty()) return;

    int verIdx = m_versionBox->currentIndex();
    if (verIdx < 0 || verIdx >= m_modVersions.size()) return;
    const auto& version = m_modVersions[verIdx];

    m_downloadBtn->setEnabled(false);
    m_dlProgress->setVisible(true);
    m_dlProgress->setValue(0);
    m_dlStatus->setText("Downloading " + version.filename + "...");

    ModSearch::instance().downloadMod(version, inst.modsDir(),
        [this](int pct){
            QMetaObject::invokeMethod(this, [this, pct](){ m_dlProgress->setValue(pct); },
                Qt::QueuedConnection);
        },
        [this, version](bool ok, const QString& err){
            QMetaObject::invokeMethod(this, [this, ok, err, version](){
                m_dlProgress->setVisible(false);
                m_downloadBtn->setEnabled(true);
                if (ok) {
                    m_dlStatus->setText("✓ Installed: " + version.filename);
                } else {
                    m_dlStatus->setText("✗ Failed: " + err);
                }
            }, Qt::QueuedConnection);
        });
}

void ModsPage::onNextPage() {
    m_currentPage++;
    onSearch();
}
void ModsPage::onPrevPage() {
    if (m_currentPage > 0) { m_currentPage--; onSearch(); }
}

void ModsPage::refreshInstalledMods() {
    m_installedList->clear();
    int instIdx = m_instanceBox->currentIndex();
    if (instIdx < 0) return;
    QString instId = m_instanceBox->itemData(instIdx).toString();
    auto inst = InstanceManager::instance().findById(instId);
    if (inst.id.isEmpty()) return;

    QDir modsDir(inst.modsDir());
    if (!modsDir.exists()) {
        m_installedList->addItem("No mods folder (install a mod or create it)");
        return;
    }

    auto files = modsDir.entryInfoList({"*.jar"}, QDir::Files);
    for (auto& fi : files) {
        auto* item = new QListWidgetItem("📦  " + fi.fileName() +
            "  (" + QString::number(fi.size() / 1024) + " KB)");
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        m_installedList->addItem(item);
    }
    if (files.isEmpty())
        m_installedList->addItem("No mods installed in this instance");
}
