#include "SettingsPage.h"
#include "../utils/AppConfig.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QFrame>

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    loadSettings();
}

void SettingsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(20);

    auto* title = new QLabel("Settings");
    title->setObjectName("pageTitle");
    root->addWidget(title);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* cLayout = new QVBoxLayout(content);
    cLayout->setSpacing(18);
    cLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(content);

    // ── Appearance ──────────────────────────────────────────────
    auto* appearGroup = new QGroupBox("Appearance");
    auto* appearForm  = new QFormLayout(appearGroup);
    appearForm->setSpacing(12);

    m_themeBox = new QComboBox;
    m_themeBox->addItem("Dark (Default)", "dark");
    m_themeBox->addItem("Light",          "light");
    m_themeBox->addItem("Deep Blue",      "deepblue");
    m_themeBox->addItem("Midnight Green", "midnight");
    m_themeBox->addItem("Nord",           "nord");
    m_themeBox->setFixedHeight(34);
    connect(m_themeBox, &QComboBox::currentIndexChanged, this, [this](){
        emit themeChanged(m_themeBox->currentData().toString());
        saveSettings();
    });
    appearForm->addRow("Theme:", m_themeBox);
    cLayout->addWidget(appearGroup);

    // ── Behaviour ───────────────────────────────────────────────
    auto* behGroup = new QGroupBox("Behaviour");
    auto* behLayout = new QVBoxLayout(behGroup);
    behLayout->setSpacing(10);

    m_minimizeTray = new QCheckBox("Minimize to system tray");
    behLayout->addWidget(m_minimizeTray);

    m_closeToTray = new QCheckBox("Close button sends to tray instead of quitting");
    behLayout->addWidget(m_closeToTray);

    m_showSnaps = new QCheckBox("Show snapshot versions by default");
    behLayout->addWidget(m_showSnaps);

    // Save any toggle
    auto saveOnToggle = [this](){ saveSettings(); };
    connect(m_minimizeTray, &QCheckBox::toggled, this, saveOnToggle);
    connect(m_closeToTray,  &QCheckBox::toggled, this, saveOnToggle);
    connect(m_showSnaps,    &QCheckBox::toggled, this, saveOnToggle);
    cLayout->addWidget(behGroup);

    // ── Storage ─────────────────────────────────────────────────
    auto* storGroup = new QGroupBox("Storage");
    auto* storForm  = new QFormLayout(storGroup);
    storForm->setSpacing(12);

    m_dataDir = new QLineEdit;
    m_dataDir->setReadOnly(true);
    m_dataDir->setFixedHeight(34);
    m_dataDir->setText(AppConfig::instance().dataDir());

    auto* dataDirRow = new QHBoxLayout;
    dataDirRow->addWidget(m_dataDir, 1);
    auto* openDataBtn = new QPushButton("Open");
    openDataBtn->setFixedHeight(34);
    connect(openDataBtn, &QPushButton::clicked, this, [this](){
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppConfig::instance().dataDir()));
    });
    dataDirRow->addWidget(openDataBtn);
    auto* dataDirW = new QWidget; dataDirW->setLayout(dataDirRow);
    storForm->addRow("Data Directory:", dataDirW);

    auto* cacheLabel = new QLabel;
    cacheLabel->setStyleSheet("color:#607d8b; font-size:11px;");
    cacheLabel->setText("All instances, assets, libraries, and Java installs are stored here.");
    storForm->addRow("", cacheLabel);

    auto* clearCacheBtn = new QPushButton("Clear Temp Files");
    clearCacheBtn->setObjectName("dangerBtn");
    clearCacheBtn->setFixedHeight(32);
    connect(clearCacheBtn, &QPushButton::clicked, this, [this](){
        auto res = QMessageBox::question(this, "Clear Temp",
            "Delete temporary download files? This will NOT remove instances or Java installs.",
            QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::Yes) {
            QDir(AppConfig::instance().tempDir()).removeRecursively();
            QDir().mkpath(AppConfig::instance().tempDir());
            QMessageBox::information(this, "Done", "Temp files cleared.");
        }
    });
    storForm->addRow("", clearCacheBtn);
    cLayout->addWidget(storGroup);

    // ── About ───────────────────────────────────────────────────
    auto* aboutGroup = new QGroupBox("About KLauncher");
    auto* aboutLayout = new QVBoxLayout(aboutGroup);

    auto* aboutText = new QLabel(
        "<b>KLauncher</b> v" KLAUNCHER_VERSION "<br>"
        "A fast, native Qt6 Minecraft launcher for Linux.<br>"
        "Licensed under <b>GPL v3</b> — free and open source.<br><br>"
        "Built with Qt " QT_VERSION_STR " • C++20");
    aboutText->setTextFormat(Qt::RichText);
    aboutText->setStyleSheet("color:#c0c8d8; font-size:13px; line-height:1.6;");
    aboutLayout->addWidget(aboutText);

    auto* linksRow = new QHBoxLayout;
    auto* ghBtn = new QPushButton("GitHub Repository");
    ghBtn->setFixedHeight(32);
    connect(ghBtn, &QPushButton::clicked, this, [](){
        QDesktopServices::openUrl(QUrl("https://github.com/Dacraezy1/KLauncher"));
    });
    linksRow->addWidget(ghBtn);

    auto* issueBtn = new QPushButton("Report Issue");
    issueBtn->setFixedHeight(32);
    connect(issueBtn, &QPushButton::clicked, this, [](){
        QDesktopServices::openUrl(QUrl("https://github.com/Dacraezy1/KLauncher/issues"));
    });
    linksRow->addWidget(issueBtn);
    linksRow->addStretch();
    aboutLayout->addLayout(linksRow);
    cLayout->addWidget(aboutGroup);

    cLayout->addStretch();
    root->addWidget(scroll, 1);
}

void SettingsPage::loadSettings() {
    auto& cfg = AppConfig::instance();
    int themeIdx = m_themeBox->findData(cfg.getString("theme", "dark"));
    if (themeIdx >= 0) m_themeBox->setCurrentIndex(themeIdx);
    m_minimizeTray->setChecked(cfg.getBool("minimizeToTray", true));
    m_closeToTray ->setChecked(cfg.getBool("minimizeToTray", true));
    m_showSnaps   ->setChecked(cfg.getBool("showSnapshots",  false));
}

void SettingsPage::saveSettings() {
    auto& cfg = AppConfig::instance();
    cfg.set("theme",           m_themeBox->currentData().toString());
    cfg.set("minimizeToTray",  m_minimizeTray->isChecked());
    cfg.set("showSnapshots",   m_showSnaps->isChecked());
}
