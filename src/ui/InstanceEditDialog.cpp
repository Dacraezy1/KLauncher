#include "InstanceEditDialog.h"
#include "../minecraft/VersionFetcher.h"
#include "../modloaders/ModLoaderInstaller.h"
#include "../java/JavaManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>
#include <QScrollArea>
#include <QProgressBar>
#include <QSlider>

InstanceEditDialog::InstanceEditDialog(const InstanceConfig& cfg, QWidget* parent)
    : QDialog(parent), m_cfg(cfg), m_editMode(!cfg.id.isEmpty())
{
    setWindowTitle(m_editMode ? "Edit Instance — " + cfg.name : "New Instance");
    setMinimumSize(560, 580);
    setModal(true);
    setupUi();
    populateVersions();
}

void InstanceEditDialog::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(16);
    root->setContentsMargins(20, 20, 20, 16);

    auto* tabs = new QTabWidget;

    // ── General Tab ────────────────────────────────────────────────
    auto* generalW = new QWidget;
    auto* gLayout  = new QFormLayout(generalW);
    gLayout->setSpacing(12);
    gLayout->setContentsMargins(16, 16, 16, 16);

    // Icon + Name row
    auto* iconNameRow = new QHBoxLayout;
    m_iconLabel = new QLabel("🎮");
    m_iconLabel->setFixedSize(56, 56);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet(
        "font-size:32px; background:rgba(15,52,96,0.5); border-radius:10px; border:1px solid #2d4a7a;");
    m_iconLabel->setCursor(Qt::PointingHandCursor);
    m_iconLabel->installEventFilter(this);
    iconNameRow->addWidget(m_iconLabel);

    m_nameEdit = new QLineEdit(m_cfg.name);
    m_nameEdit->setPlaceholderText("Instance name...");
    m_nameEdit->setFixedHeight(38);
    iconNameRow->addWidget(m_nameEdit, 1);

    auto* iconNameW = new QWidget;
    iconNameW->setLayout(iconNameRow);
    gLayout->addRow("Name:", iconNameW);

    // MC Version
    auto* versionRow = new QHBoxLayout;
    m_mcVersionBox = new QComboBox;
    m_mcVersionBox->setFixedHeight(34);
    m_mcVersionBox->setMinimumWidth(160);
    versionRow->addWidget(m_mcVersionBox, 1);

    m_showSnaps = new QCheckBox("Show snapshots");
    m_showSnaps->setChecked(false);
    connect(m_showSnaps, &QCheckBox::toggled, this, [this](){ populateVersions(); });
    versionRow->addWidget(m_showSnaps);

    auto* versionW = new QWidget; versionW->setLayout(versionRow);
    gLayout->addRow("MC Version:", versionW);

    connect(m_mcVersionBox, &QComboBox::currentTextChanged, this,
            &InstanceEditDialog::onMcVersionChanged);

    // Mod loader
    m_loaderBox = new QComboBox;
    m_loaderBox->setFixedHeight(34);
    m_loaderBox->addItem("None (Vanilla)",  0);
    m_loaderBox->addItem("Fabric",          1);
    m_loaderBox->addItem("Forge",           2);
    m_loaderBox->addItem("NeoForge",        3);
    m_loaderBox->addItem("Quilt",           4);
    m_loaderBox->setCurrentIndex((int)m_cfg.modLoader);
    connect(m_loaderBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InstanceEditDialog::onModLoaderChanged);
    gLayout->addRow("Mod Loader:", m_loaderBox);

    // Loader version
    auto* loaderVerRow = new QHBoxLayout;
    m_loaderVerBox = new QComboBox;
    m_loaderVerBox->setFixedHeight(34);
    m_loaderVerBox->setMinimumWidth(200);
    m_loaderVerBox->setEnabled(false);
    loaderVerRow->addWidget(m_loaderVerBox, 1);

    m_loaderStatus = new QLabel;
    m_loaderStatus->setStyleSheet("color: #90caf9; font-size: 11px;");
    loaderVerRow->addWidget(m_loaderStatus);

    auto* loaderVerW = new QWidget; loaderVerW->setLayout(loaderVerRow);
    gLayout->addRow("Loader Version:", loaderVerW);

    m_loaderProgress = new QProgressBar;
    m_loaderProgress->setFixedHeight(6);
    m_loaderProgress->setRange(0, 0);
    m_loaderProgress->setVisible(false);
    gLayout->addRow("", m_loaderProgress);

    // Group
    m_groupEdit = new QLineEdit(m_cfg.group);
    m_groupEdit->setPlaceholderText("Optional group name...");
    m_groupEdit->setFixedHeight(34);
    gLayout->addRow("Group:", m_groupEdit);

    // Notes
    m_notesEdit = new QTextEdit(m_cfg.notes);
    m_notesEdit->setPlaceholderText("Optional notes...");
    m_notesEdit->setFixedHeight(70);
    gLayout->addRow("Notes:", m_notesEdit);

    tabs->addTab(generalW, "⚙  General");

    // ── Java Tab ──────────────────────────────────────────────────
    auto* javaW   = new QWidget;
    auto* jLayout = new QVBoxLayout(javaW);
    jLayout->setContentsMargins(16, 16, 16, 16);
    jLayout->setSpacing(14);

    auto* javaGroup = new QGroupBox("Java Executable");
    auto* javaGLayout = new QVBoxLayout(javaGroup);

    auto* javaPathRow = new QHBoxLayout;
    m_javaPath = new QLineEdit(m_cfg.javaPath);
    m_javaPath->setPlaceholderText("Auto-detect (recommended)");
    m_javaPath->setFixedHeight(34);
    javaPathRow->addWidget(m_javaPath, 1);

    auto* browseJavaBtn = new QPushButton("Browse");
    browseJavaBtn->setFixedHeight(34);
    connect(browseJavaBtn, &QPushButton::clicked, this, &InstanceEditDialog::onBrowseJava);
    javaPathRow->addWidget(browseJavaBtn);
    javaGLayout->addLayout(javaPathRow);

    m_javaDetected = new QLabel;
    m_javaDetected->setStyleSheet("color: #69f0ae; font-size: 11px;");
    javaGLayout->addWidget(m_javaDetected);

    // Populate detected java
    auto javas = JavaManager::instance().installs();
    if (!javas.isEmpty()) {
        QStringList javaStrs;
        for (auto& j : javas)
            javaStrs << QString("Java %1 — %2").arg(j.majorVersion).arg(j.path);
        m_javaDetected->setText("Detected: " + javaStrs.join(", ").left(80) + (javaStrs.join("").length() > 80 ? "…" : ""));
    } else {
        m_javaDetected->setText("⚠ No Java detected — will auto-find system java");
    }
    jLayout->addWidget(javaGroup);

    // RAM group
    auto* ramGroup = new QGroupBox("Memory");
    auto* ramGLayout = new QFormLayout(ramGroup);
    ramGLayout->setSpacing(10);

    m_minRam = new QSpinBox;
    m_minRam->setRange(256, 32768);
    m_minRam->setSingleStep(256);
    m_minRam->setValue(m_cfg.minRam);
    m_minRam->setSuffix(" MB");
    m_minRam->setFixedHeight(34);
    ramGLayout->addRow("Min RAM:", m_minRam);

    m_maxRam = new QSpinBox;
    m_maxRam->setRange(512, 65536);
    m_maxRam->setSingleStep(256);
    m_maxRam->setValue(m_cfg.maxRam);
    m_maxRam->setSuffix(" MB");
    m_maxRam->setFixedHeight(34);
    ramGLayout->addRow("Max RAM:", m_maxRam);

    m_ramPreview = new QLabel;
    m_ramPreview->setStyleSheet("color: #90caf9; font-size: 11px;");
    ramGLayout->addRow("", m_ramPreview);

    connect(m_minRam, QOverload<int>::of(&QSpinBox::valueChanged), this, &InstanceEditDialog::onRamChanged);
    connect(m_maxRam, QOverload<int>::of(&QSpinBox::valueChanged), this, &InstanceEditDialog::onRamChanged);
    onRamChanged();

    // Quick presets
    auto* presetsRow = new QHBoxLayout;
    for (auto& preset : QList<QPair<QString,int>>{{"1 GB",1024},{"2 GB",2048},{"4 GB",4096},{"6 GB",6144},{"8 GB",8192}}) {
        auto* btn = new QPushButton(preset.first);
        btn->setFixedHeight(28);
        btn->setFixedWidth(54);
        connect(btn, &QPushButton::clicked, this, [this, val = preset.second](){
            m_maxRam->setValue(val);
            m_minRam->setValue(qMin(m_minRam->value(), val / 2));
        });
        presetsRow->addWidget(btn);
    }
    presetsRow->addStretch();
    ramGLayout->addRow("Presets:", [&]() -> QWidget* {
        auto* w = new QWidget; w->setLayout(presetsRow); return w;
    }());
    jLayout->addWidget(ramGroup);

    // JVM Args
    auto* jvmGroup = new QGroupBox("Extra JVM Arguments");
    auto* jvmGLayout = new QVBoxLayout(jvmGroup);
    m_jvmArgs = new QTextEdit(m_cfg.jvmArgs);
    m_jvmArgs->setPlaceholderText(
        "-XX:+UseShenandoahGC -XX:ShenandoahGCHeuristics=compact\n"
        "(Leave empty for recommended defaults)");
    m_jvmArgs->setFixedHeight(80);
    jvmGLayout->addWidget(m_jvmArgs);

    auto* jvmHint = new QLabel(
        "Recommended flags are applied automatically. Only add extra flags here.");
    jvmHint->setStyleSheet("color: #607d8b; font-size: 11px;");
    jvmHint->setWordWrap(true);
    jvmGLayout->addWidget(jvmHint);
    jLayout->addWidget(jvmGroup);
    jLayout->addStretch();
    tabs->addTab(javaW, "☕  Java");

    // ── Window Tab ────────────────────────────────────────────────
    auto* winW   = new QWidget;
    auto* wLayout = new QFormLayout(winW);
    wLayout->setContentsMargins(16, 16, 16, 16);
    wLayout->setSpacing(12);

    m_resolution = new QLineEdit(m_cfg.resolution);
    m_resolution->setPlaceholderText("e.g. 1280x720  (empty = default)");
    m_resolution->setFixedHeight(34);
    wLayout->addRow("Resolution:", m_resolution);

    m_fullscreen = new QCheckBox("Start in fullscreen");
    m_fullscreen->setChecked(m_cfg.fullscreen);
    wLayout->addRow("", m_fullscreen);

    m_gameDir = new QLineEdit(m_cfg.gameDir);
    m_gameDir->setPlaceholderText("Default: instance folder");
    m_gameDir->setFixedHeight(34);
    auto* gameDirRow = new QHBoxLayout;
    gameDirRow->addWidget(m_gameDir, 1);
    auto* browseGameDir = new QPushButton("Browse");
    browseGameDir->setFixedHeight(34);
    connect(browseGameDir, &QPushButton::clicked, this, [this](){
        QString d = QFileDialog::getExistingDirectory(this, "Game Directory", m_gameDir->text());
        if (!d.isEmpty()) m_gameDir->setText(d);
    });
    gameDirRow->addWidget(browseGameDir);
    auto* gameDirW = new QWidget; gameDirW->setLayout(gameDirRow);
    wLayout->addRow("Game Directory:", gameDirW);
    tabs->addTab(winW, "🖥  Window");

    root->addWidget(tabs);

    // Dialog buttons
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnBox->button(QDialogButtonBox::Ok)->setText(m_editMode ? "Save" : "Create");
    btnBox->button(QDialogButtonBox::Ok)->setFixedHeight(36);
    btnBox->button(QDialogButtonBox::Cancel)->setFixedHeight(36);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this](){
        if (m_nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Validation", "Instance name cannot be empty.");
            return;
        }
        if (m_mcVersionBox->currentText().isEmpty()) {
            QMessageBox::warning(this, "Validation", "Please select a Minecraft version.");
            return;
        }
        accept();
    });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btnBox);
}

void InstanceEditDialog::populateVersions() {
    QString current = m_mcVersionBox->currentText().isEmpty()
                    ? m_cfg.mcVersion
                    : m_mcVersionBox->currentText();

    auto doPopulate = [this, current](){
        m_mcVersionBox->blockSignals(true);
        m_mcVersionBox->clear();
        bool showSnaps = m_showSnaps->isChecked();
        for (auto& v : VersionFetcher::instance().versions()) {
            if (!showSnaps && v.type != "release") continue;
            QString label = v.id;
            if (v.type == "snapshot") label += " (snapshot)";
            else if (v.type == "old_beta") label += " (old beta)";
            m_mcVersionBox->addItem(label, v.id);
        }
        int idx = m_mcVersionBox->findData(current);
        if (idx >= 0) m_mcVersionBox->setCurrentIndex(idx);
        else if (m_mcVersionBox->count() > 0) m_mcVersionBox->setCurrentIndex(0);
        m_mcVersionBox->blockSignals(false);
        if (m_cfg.modLoader != ModLoaderType::None)
            onMcVersionChanged(m_mcVersionBox->currentText());
    };

    if (VersionFetcher::instance().versions().isEmpty()) {
        m_mcVersionBox->addItem("Loading versions...");
        m_mcVersionBox->setEnabled(false);
        VersionFetcher::instance().fetchVersionList([this, doPopulate](bool ok){
            QMetaObject::invokeMethod(this, [this, doPopulate, ok](){
                m_mcVersionBox->setEnabled(ok);
                if (ok) doPopulate();
                else m_mcVersionBox->setItemText(0, "Failed to load versions");
            }, Qt::QueuedConnection);
        });
    } else {
        doPopulate();
    }
}

void InstanceEditDialog::onMcVersionChanged(const QString&) {
    int loaderIdx = m_loaderBox->currentIndex();
    if (loaderIdx == 0) return;
    onModLoaderChanged(loaderIdx);
}

void InstanceEditDialog::onModLoaderChanged(int index) {
    m_loaderVerBox->clear();
    m_loaderVerBox->setEnabled(false);
    m_loaderStatus->clear();

    if (index == 0) return;

    QString mcVer = m_mcVersionBox->currentData().toString();
    if (mcVer.isEmpty()) mcVer = m_mcVersionBox->currentText();
    if (mcVer.isEmpty()) return;

    m_loaderProgress->setVisible(true);
    m_loaderStatus->setText("Fetching versions...");

    auto handleVersions = [this](bool ok, const QList<ModLoaderVersion>& list){
        QMetaObject::invokeMethod(this, [this, ok, list](){
            m_loaderProgress->setVisible(false);
            if (!ok || list.isEmpty()) {
                m_loaderStatus->setText("No versions found");
                return;
            }
            m_loaderVerBox->blockSignals(true);
            for (auto& v : list) {
                QString label = v.loaderVersion;
                if (!v.stable) label += " (unstable)";
                m_loaderVerBox->addItem(label, v.loaderVersion);
            }
            // Select existing if editing
            if (!m_cfg.modLoaderVersion.isEmpty()) {
                int idx = m_loaderVerBox->findData(m_cfg.modLoaderVersion);
                if (idx >= 0) m_loaderVerBox->setCurrentIndex(idx);
            }
            m_loaderVerBox->blockSignals(false);
            m_loaderVerBox->setEnabled(true);
            m_loaderStatus->setText(QString("%1 versions").arg(list.size()));
        }, Qt::QueuedConnection);
    };

    if (index == 1) // Fabric
        ModLoaderInstaller::instance().fetchFabricVersions(mcVer,
            [handleVersions](bool ok, QList<ModLoaderVersion> list){ handleVersions(ok, list); });
    else if (index == 2) // Forge
        ModLoaderInstaller::instance().fetchForgeVersions(mcVer,
            [handleVersions](bool ok, QList<ModLoaderVersion> list){ handleVersions(ok, list); });
    else if (index == 3) // NeoForge
        ModLoaderInstaller::instance().fetchNeoForgeVersions(mcVer,
            [handleVersions](bool ok, QList<ModLoaderVersion> list){ handleVersions(ok, list); });
    else if (index == 4) // Quilt
        ModLoaderInstaller::instance().fetchQuiltVersions(mcVer,
            [handleVersions](bool ok, QList<ModLoaderVersion> list){ handleVersions(ok, list); });
}

void InstanceEditDialog::onBrowseJava() {
    QString path = QFileDialog::getOpenFileName(this, "Select Java Executable",
        "/usr/bin", "Java (java*);;All files (*)");
    if (!path.isEmpty()) m_javaPath->setText(path);
}

void InstanceEditDialog::onBrowseIcon() {
    QString path = QFileDialog::getOpenFileName(this, "Select Icon",
        "", "Images (*.png *.jpg *.jpeg *.svg)");
    if (!path.isEmpty()) {
        m_iconPath = path;
        QPixmap px(path);
        if (!px.isNull())
            m_iconLabel->setPixmap(px.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void InstanceEditDialog::onRamChanged() {
    if (!m_minRam || !m_maxRam || !m_ramPreview) return;
    int minV = m_minRam->value(), maxV = m_maxRam->value();
    if (minV > maxV) m_maxRam->setValue(minV);
    m_ramPreview->setText(QString("Allocated: %1 MB – %2 MB  (%3 GB – %4 GB)")
        .arg(minV).arg(maxV)
        .arg(minV / 1024.0, 0, 'f', 1)
        .arg(maxV / 1024.0, 0, 'f', 1));
}

bool InstanceEditDialog::eventFilter(QObject* obj, QEvent* e) {
    if (obj == m_iconLabel && e->type() == QEvent::MouseButtonPress) {
        onBrowseIcon();
        return true;
    }
    return QDialog::eventFilter(obj, e);
}

InstanceConfig InstanceEditDialog::config() const {
    InstanceConfig c = m_cfg;
    c.name      = m_nameEdit->text().trimmed();
    c.mcVersion = m_mcVersionBox->currentData().toString();
    if (c.mcVersion.isEmpty()) c.mcVersion = m_mcVersionBox->currentText();
    c.modLoader = (ModLoaderType)m_loaderBox->currentIndex();
    c.modLoaderVersion = m_loaderVerBox->currentData().toString();
    c.javaPath  = m_javaPath->text().trimmed();
    c.minRam    = m_minRam->value();
    c.maxRam    = m_maxRam->value();
    c.jvmArgs   = m_jvmArgs->toPlainText().trimmed();
    c.group     = m_groupEdit->text().trimmed();
    c.notes     = m_notesEdit->toPlainText();
    c.resolution= m_resolution->text().trimmed();
    c.fullscreen= m_fullscreen->isChecked();
    c.gameDir   = m_gameDir->text().trimmed();
    if (!m_iconPath.isEmpty()) c.iconPath = m_iconPath;
    return c;
}
