#include "InstancesPage.h"
#include "InstanceEditDialog.h"
#include "ConsoleWidget.h"
#include "../minecraft/InstanceManager.h"
#include "../minecraft/GameLauncher.h"
#include "../minecraft/VersionFetcher.h"
#include "../auth/MicrosoftAuth.h"
#include "../utils/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QLabel>
#include <QFont>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QDateTime>
#include <QDebug>

InstancesPage::InstancesPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshList();

    connect(&InstanceManager::instance(), &InstanceManager::instancesChanged,
            this, &InstancesPage::onInstancesChanged);
    connect(&GameLauncher::instance(), &GameLauncher::gameStarted,
            this, [this](const QString& id){
                if (id == m_selectedId) {
                    m_launchBtn->setVisible(false);
                    m_killBtn->setVisible(true);
                    m_installStatus->setText("▶ Game running");
                    m_installProgress->setVisible(false);
                }
            });
    connect(&GameLauncher::instance(), &GameLauncher::gameStopped,
            this, [this](const QString& id, int code){
                if (id == m_selectedId) {
                    m_launchBtn->setVisible(true);
                    m_killBtn->setVisible(false);
                    m_installStatus->setText(code == 0 ? "✓ Stopped cleanly" : "⚠ Exited with code " + QString::number(code));
                }
                refreshList();
            });
    connect(&GameLauncher::instance(), &GameLauncher::gameLog,
            this, [this](const QString& id, const QString& line){
                if (id == m_selectedId && m_console)
                    m_console->appendLine(line);
            });
}

void InstancesPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(16);

    // Header
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("Instances");
    title->setObjectName("pageTitle");
    header->addWidget(title);
    header->addStretch();

    auto* createBtn = new QPushButton("＋  New Instance");
    createBtn->setFixedHeight(36);
    connect(createBtn, &QPushButton::clicked, this, &InstancesPage::onCreateClicked);
    header->addWidget(createBtn);
    root->addLayout(header);

    // Search & filter row
    auto* filterRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText("🔍  Search instances...");
    m_search->setFixedHeight(34);
    connect(m_search, &QLineEdit::textChanged, this, &InstancesPage::onFilterChanged);
    filterRow->addWidget(m_search, 1);

    m_groupFilter = new QComboBox;
    m_groupFilter->addItem("All Groups");
    m_groupFilter->setFixedHeight(34);
    m_groupFilter->setMinimumWidth(140);
    connect(m_groupFilter, &QComboBox::currentTextChanged, this, &InstancesPage::onFilterChanged);
    filterRow->addWidget(m_groupFilter);
    root->addLayout(filterRow);

    // Main splitter: list | detail + console
    m_splitter = new QSplitter(Qt::Horizontal);

    // Left: instance list
    m_list = new QListWidget;
    m_list->setObjectName("instanceList");
    m_list->setIconSize(QSize(48, 48));
    m_list->setSpacing(4);
    m_list->setMinimumWidth(220);
    connect(m_list, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem* cur, QListWidgetItem*){
            onInstanceSelected(cur);
        });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*){
        onLaunchClicked();
    });
    m_splitter->addWidget(m_list);

    // Right: detail + console stacked vertically
    auto* rightSplitter = new QSplitter(Qt::Vertical);

    // Detail panel
    m_detailPanel = new QWidget;
    m_detailPanel->setObjectName("detailPanel");
    m_detailPanel->setMinimumHeight(200);
    auto* detailLayout = new QVBoxLayout(m_detailPanel);
    detailLayout->setContentsMargins(20, 16, 20, 16);
    detailLayout->setSpacing(12);

    // Icon + name row
    auto* nameRow = new QHBoxLayout;
    m_instIcon = new QLabel;
    m_instIcon->setFixedSize(64, 64);
    m_instIcon->setObjectName("instanceIcon");
    m_instIcon->setAlignment(Qt::AlignCenter);
    m_instIcon->setText("🎮");
    m_instIcon->setStyleSheet("font-size: 40px; background: rgba(15,52,96,0.5); border-radius: 12px;");
    nameRow->addWidget(m_instIcon);

    auto* nameCol = new QVBoxLayout;
    m_instName = new QLabel("Select an instance");
    m_instName->setObjectName("instanceName");
    m_instName->setStyleSheet("font-size: 20px; font-weight: 700;");
    nameCol->addWidget(m_instName);
    m_instMeta = new QLabel;
    m_instMeta->setObjectName("instanceMeta");
    m_instMeta->setStyleSheet("color: #90caf9; font-size: 12px;");
    nameCol->addWidget(m_instMeta);
    nameRow->addLayout(nameCol, 1);
    detailLayout->addLayout(nameRow);

    // Action buttons
    auto* btnRow = new QHBoxLayout;
    m_launchBtn = new QPushButton("▶  Launch");
    m_launchBtn->setObjectName("launchBtn");
    m_launchBtn->setFixedHeight(40);
    m_launchBtn->setEnabled(false);
    connect(m_launchBtn, &QPushButton::clicked, this, &InstancesPage::onLaunchClicked);
    btnRow->addWidget(m_launchBtn);

    m_killBtn = new QPushButton("⏹  Kill");
    m_killBtn->setObjectName("dangerBtn");
    m_killBtn->setFixedHeight(40);
    m_killBtn->setVisible(false);
    connect(m_killBtn, &QPushButton::clicked, this, &InstancesPage::onKillClicked);
    btnRow->addWidget(m_killBtn);

    m_editBtn = new QPushButton("✏  Edit");
    m_editBtn->setFixedHeight(40);
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QPushButton::clicked, this, &InstancesPage::onEditClicked);
    btnRow->addWidget(m_editBtn);

    m_dupBtn = new QPushButton("⧉  Copy");
    m_dupBtn->setFixedHeight(40);
    m_dupBtn->setEnabled(false);
    connect(m_dupBtn, &QPushButton::clicked, this, &InstancesPage::onDuplicateClicked);
    btnRow->addWidget(m_dupBtn);

    m_deleteBtn = new QPushButton("🗑  Delete");
    m_deleteBtn->setObjectName("dangerBtn");
    m_deleteBtn->setFixedHeight(40);
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &InstancesPage::onDeleteClicked);
    btnRow->addWidget(m_deleteBtn);
    detailLayout->addLayout(btnRow);

    // Install progress
    m_installProgress = new QProgressBar;
    m_installProgress->setVisible(false);
    m_installProgress->setFixedHeight(10);
    detailLayout->addWidget(m_installProgress);

    m_installStatus = new QLabel;
    m_installStatus->setObjectName("installStatus");
    m_installStatus->setStyleSheet("color: #90caf9; font-size: 12px;");
    detailLayout->addWidget(m_installStatus);
    detailLayout->addStretch();

    rightSplitter->addWidget(m_detailPanel);

    // Console
    m_console = new ConsoleWidget;
    rightSplitter->addWidget(m_console);
    rightSplitter->setSizes({300, 200});

    m_splitter->addWidget(rightSplitter);
    m_splitter->setSizes({280, 700});

    root->addWidget(m_splitter, 1);
}

void InstancesPage::refreshList() {
    QString filter = m_search ? m_search->text().toLower() : "";
    QString group  = m_groupFilter ? m_groupFilter->currentText() : "All Groups";

    // Update group filter options
    if (m_groupFilter) {
        QString current = m_groupFilter->currentText();
        m_groupFilter->blockSignals(true);
        m_groupFilter->clear();
        m_groupFilter->addItem("All Groups");
        QSet<QString> groups;
        for (auto& inst : InstanceManager::instance().instances())
            if (!inst.group.isEmpty()) groups.insert(inst.group);
        for (auto& g : groups) m_groupFilter->addItem(g);
        int idx = m_groupFilter->findText(current);
        m_groupFilter->setCurrentIndex(idx >= 0 ? idx : 0);
        m_groupFilter->blockSignals(false);
        group = m_groupFilter->currentText();
    }

    m_list->clear();
    for (auto& inst : InstanceManager::instance().instances()) {
        if (!filter.isEmpty() && !inst.name.toLower().contains(filter)) continue;
        if (group != "All Groups" && inst.group != group) continue;

        auto* item = new QListWidgetItem;
        QString loaderStr;
        switch (inst.modLoader) {
            case ModLoaderType::Fabric:   loaderStr = " [Fabric]";   break;
            case ModLoaderType::Forge:    loaderStr = " [Forge]";    break;
            case ModLoaderType::NeoForge: loaderStr = " [NeoForge]"; break;
            case ModLoaderType::Quilt:    loaderStr = " [Quilt]";    break;
            default: break;
        }
        item->setText(inst.name + "\n" + inst.mcVersion + loaderStr);
        item->setData(Qt::UserRole, inst.id);

        bool running = GameLauncher::instance().isRunning(inst.id);
        if (running) item->setForeground(QColor("#69f0ae"));

        m_list->addItem(item);
        if (inst.id == m_selectedId) m_list->setCurrentItem(item);
    }

    if (m_list->count() == 0) {
        auto* placeholder = new QListWidgetItem("No instances yet.\nClick '+  New Instance' to create one.");
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setForeground(QColor("#4a5568"));
        placeholder->setTextAlignment(Qt::AlignCenter);
        m_list->addItem(placeholder);
    }
}

void InstancesPage::onInstancesChanged() { refreshList(); }
void InstancesPage::onFilterChanged(const QString&) { refreshList(); }

void InstancesPage::onInstanceSelected(QListWidgetItem* item) {
    if (!item || !item->flags().testFlag(Qt::ItemIsEnabled)) {
        m_selectedId.clear();
        m_launchBtn->setEnabled(false);
        m_editBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        m_dupBtn->setEnabled(false);
        return;
    }
    m_selectedId = item->data(Qt::UserRole).toString();
    auto inst = InstanceManager::instance().findById(m_selectedId);
    if (inst.id.isEmpty()) return;

    updateDetailPanel(inst);
    bool running = GameLauncher::instance().isRunning(m_selectedId);
    m_launchBtn->setEnabled(!running);
    m_launchBtn->setVisible(!running);
    m_killBtn->setVisible(running);
    m_editBtn->setEnabled(true);
    m_deleteBtn->setEnabled(!running);
    m_dupBtn->setEnabled(true);
}

void InstancesPage::updateDetailPanel(const InstanceConfig& inst) {
    m_instName->setText(inst.name);

    QString loaderStr = "Vanilla";
    switch (inst.modLoader) {
        case ModLoaderType::Fabric:   loaderStr = "Fabric " + inst.modLoaderVersion; break;
        case ModLoaderType::Forge:    loaderStr = "Forge " + inst.modLoaderVersion;  break;
        case ModLoaderType::NeoForge: loaderStr = "NeoForge " + inst.modLoaderVersion; break;
        case ModLoaderType::Quilt:    loaderStr = "Quilt " + inst.modLoaderVersion;  break;
        default: break;
    }

    QString meta = QString("Minecraft %1  •  %2  •  RAM: %3–%4 MB")
        .arg(inst.mcVersion, loaderStr)
        .arg(inst.minRam).arg(inst.maxRam);
    if (inst.lastPlayed.isValid())
        meta += "\nLast played: " + inst.lastPlayed.toString("yyyy-MM-dd hh:mm");
    if (inst.playTime > 0) {
        int h = inst.playTime / 3600, m = (inst.playTime % 3600) / 60;
        meta += QString("  •  Play time: %1h %2m").arg(h).arg(m);
    }
    m_instMeta->setText(meta);
    m_installStatus->clear();
}

void InstancesPage::onCreateClicked() {
    auto* dlg = new InstanceEditDialog(InstanceConfig{}, this);
    if (dlg->exec() == QDialog::Accepted) {
        InstanceManager::instance().createInstance(dlg->config());
    }
    dlg->deleteLater();
}

void InstancesPage::onEditClicked() {
    auto inst = InstanceManager::instance().findById(m_selectedId);
    if (inst.id.isEmpty()) return;
    auto* dlg = new InstanceEditDialog(inst, this);
    if (dlg->exec() == QDialog::Accepted) {
        InstanceManager::instance().updateInstance(dlg->config());
        updateDetailPanel(dlg->config());
    }
    dlg->deleteLater();
}

void InstancesPage::onDeleteClicked() {
    auto inst = InstanceManager::instance().findById(m_selectedId);
    if (inst.id.isEmpty()) return;
    auto res = QMessageBox::question(this, "Delete Instance",
        "Delete instance \"" + inst.name + "\"?\nThis will remove all instance data.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        InstanceManager::instance().deleteInstance(m_selectedId);
        m_selectedId.clear();
        m_launchBtn->setEnabled(false);
        m_editBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        m_dupBtn->setEnabled(false);
        m_instName->setText("Select an instance");
        m_instMeta->clear();
    }
}

void InstancesPage::onDuplicateClicked() {
    InstanceManager::instance().duplicateInstance(m_selectedId);
}

void InstancesPage::onLaunchClicked() {
    if (m_selectedId.isEmpty()) return;
    installAndLaunch(InstanceManager::instance().findById(m_selectedId));
}

void InstancesPage::onKillClicked() {
    GameLauncher::instance().kill(m_selectedId);
}

void InstancesPage::installAndLaunch(const InstanceConfig& inst) {
    if (inst.id.isEmpty()) return;

    // Check if version is installed, if not — download it first
    if (!VersionFetcher::instance().isInstalled(inst.mcVersion)) {
        m_launchBtn->setEnabled(false);
        m_installProgress->setVisible(true);
        m_installProgress->setRange(0, 0); // busy
        m_installStatus->setText("⬇ Downloading Minecraft " + inst.mcVersion + "...");

        // Fetch version list first if empty
        auto doDownload = [this, inst](){
            VersionFetcher::instance().downloadVersion(inst.mcVersion,
                [this, inst](bool ok, const QString& err){
                    QMetaObject::invokeMethod(this, [this, ok, err, inst](){
                        m_installProgress->setRange(0, 100);
                        if (!ok) {
                            m_installStatus->setText("✗ Download failed: " + err);
                            m_installProgress->setVisible(false);
                            m_launchBtn->setEnabled(true);
                            return;
                        }
                        m_installStatus->setText("✓ Download complete. Launching...");
                        m_installProgress->setVisible(false);
                        m_launchBtn->setEnabled(true);
                        doActualLaunch(inst);
                    }, Qt::QueuedConnection);
                });
        };

        if (VersionFetcher::instance().versions().isEmpty()) {
            VersionFetcher::instance().fetchVersionList([doDownload](bool){ doDownload(); });
        } else {
            doDownload();
        }
    } else {
        doActualLaunch(inst);
    }
}

void InstancesPage::doActualLaunch(const InstanceConfig& inst) {
    // Check account
    auto acc = MicrosoftAuth::instance().activeAccount();
    bool onlineMode = acc.valid && !acc.accessToken.isEmpty();

    if (!onlineMode) {
        // Offer offline mode
        bool ok;
        QString username = QInputDialog::getText(this, "Offline Mode",
            "No Microsoft account signed in.\nEnter your username for offline play:",
            QLineEdit::Normal, AppConfig::instance().getString("lastOfflineName", "Player"), &ok);
        if (!ok) return;
        AppConfig::instance().set("lastOfflineName", username);
        m_console->clear();
        GameLauncher::instance().launch(inst, {}, username);
    } else {
        // Check token expiry
        qint64 now = QDateTime::currentSecsSinceEpoch();
        if (acc.expiresAt < now + 60) {
            m_installStatus->setText("⟳ Refreshing token...");
            MicrosoftAuth::instance().refreshToken(acc, [this, inst](bool ok, MicrosoftAccount newAcc){
                QMetaObject::invokeMethod(this, [this, ok, inst, newAcc](){
                    if (!ok) {
                        m_installStatus->setText("⚠ Token refresh failed, launching offline...");
                    }
                    m_console->clear();
                    GameLauncher::instance().launch(inst, newAcc, {});
                }, Qt::QueuedConnection);
            });
            return;
        }
        m_console->clear();
        GameLauncher::instance().launch(inst, acc, {});
    }

    // Update last played
    InstanceConfig updated = inst;
    updated.lastPlayed = QDateTime::currentDateTime();
    InstanceManager::instance().updateInstance(updated);
}
