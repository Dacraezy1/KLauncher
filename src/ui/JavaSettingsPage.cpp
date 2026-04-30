#include "JavaSettingsPage.h"
#include "../java/JavaManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QFrame>

JavaSettingsPage::JavaSettingsPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshJavaList();
    connect(&JavaManager::instance(), &JavaManager::detectFinished,
            this, &JavaSettingsPage::refreshJavaList);
}

void JavaSettingsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(20);

    auto* title = new QLabel("Java Management");
    title->setObjectName("pageTitle");
    root->addWidget(title);

    // Info label
    auto* info = new QLabel(
        "KLauncher auto-detects Java installations on your system. "
        "You can also download specific versions via Adoptium (Eclipse Temurin).");
    info->setWordWrap(true);
    info->setStyleSheet("color:#90caf9; font-size:13px;"
        "background:rgba(79,195,247,0.06); border-radius:8px; padding:10px 14px;"
        "border:1px solid rgba(79,195,247,0.2);");
    root->addWidget(info);

    // Detected Java group
    auto* detectedGroup = new QGroupBox("Detected Java Installations");
    auto* detectedLayout = new QVBoxLayout(detectedGroup);

    m_javaList = new QListWidget;
    m_javaList->setMinimumHeight(160);
    m_javaList->setSpacing(2);
    detectedLayout->addWidget(m_javaList);

    auto* detectRow = new QHBoxLayout;
    m_detectBtn = new QPushButton("🔍  Re-scan System");
    m_detectBtn->setFixedHeight(34);
    connect(m_detectBtn, &QPushButton::clicked, this, &JavaSettingsPage::onDetect);
    detectRow->addWidget(m_detectBtn);
    detectRow->addStretch();
    detectedLayout->addLayout(detectRow);
    root->addWidget(detectedGroup);

    // Download Java group
    auto* dlGroup = new QGroupBox("Download Java (Adoptium Temurin)");
    auto* dlLayout = new QVBoxLayout(dlGroup);

    auto* dlHint = new QLabel(
        "Download Java JRE for your instances. Minecraft 1.17+ requires Java 17, "
        "1.20.5+ requires Java 21. Older versions work with Java 8.");
    dlHint->setWordWrap(true);
    dlHint->setStyleSheet("color:#607d8b; font-size:12px;");
    dlLayout->addWidget(dlHint);

    // Version buttons grid
    auto* btnGrid = new QHBoxLayout;
    btnGrid->setSpacing(10);
    struct JVer { int ver; QString label; QString hint; };
    for (auto& jv : QList<JVer>{
        {8,  "Java 8",  "MC ≤ 1.16"},
        {11, "Java 11", "Some modpacks"},
        {17, "Java 17", "MC 1.17 – 1.20.4"},
        {21, "Java 21", "MC 1.20.5 – 1.21"},
        {22, "Java 22", "Experimental"},
        {23, "Java 23", "Latest stable"},
        {24, "Java 24", "MC 1.21.5+"},
    }) {
        auto* card = new QWidget;
        card->setStyleSheet(
            "background:rgba(15,52,96,0.4); border:1px solid #2d4a7a; border-radius:10px; padding:6px;");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);
        cardLayout->setSpacing(4);

        auto* verLabel = new QLabel(jv.label);
        verLabel->setAlignment(Qt::AlignCenter);
        verLabel->setStyleSheet("font-size:15px; font-weight:700; color:#4fc3f7;");
        cardLayout->addWidget(verLabel);

        auto* hintLabel = new QLabel(jv.hint);
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setStyleSheet("font-size:11px; color:#607d8b;");
        cardLayout->addWidget(hintLabel);

        auto* dlBtn = new QPushButton("Download");
        dlBtn->setFixedHeight(28);
        dlBtn->setStyleSheet("font-size:12px;");
        int ver = jv.ver;
        connect(dlBtn, &QPushButton::clicked, this, [this, ver](){
            onDownloadJava(ver);
        });
        cardLayout->addWidget(dlBtn);
        btnGrid->addWidget(card);
    }
    dlLayout->addLayout(btnGrid);

    m_dlProgress = new QProgressBar;
    m_dlProgress->setFixedHeight(8);
    m_dlProgress->setVisible(false);
    dlLayout->addWidget(m_dlProgress);

    m_dlStatus = new QLabel;
    m_dlStatus->setStyleSheet("color:#69f0ae; font-size:12px;");
    dlLayout->addWidget(m_dlStatus);
    root->addWidget(dlGroup);

    root->addStretch();
}

void JavaSettingsPage::refreshJavaList() {
    m_javaList->clear();
    auto installs = JavaManager::instance().installs();
    if (installs.isEmpty()) {
        m_javaList->addItem("⚠  No Java found. Download one below or install via your package manager.");
        return;
    }
    for (auto& j : installs) {
        QString bundled = j.bundled ? "  [bundled]" : "";
        auto* item = new QListWidgetItem(
            QString("☕  Java %1%2   —   %3")
                .arg(j.majorVersion).arg(bundled).arg(j.path));
        item->setForeground(j.bundled ? QColor("#69f0ae") : QColor("#e0e0e0"));
        m_javaList->addItem(item);
    }
}

void JavaSettingsPage::onDetect() {
    m_detectBtn->setEnabled(false);
    m_dlStatus->setText("Scanning...");
    JavaManager::instance().detectJava();
    m_detectBtn->setEnabled(true);
    m_dlStatus->setText("Scan complete");
}

void JavaSettingsPage::onDownloadJava(int majorVersion) {
    m_dlStatus->setText(QString("⬇ Downloading Java %1...").arg(majorVersion));
    m_dlProgress->setVisible(true);
    m_dlProgress->setValue(0);

    JavaManager::instance().downloadJava(majorVersion,
        [this](int pct){
            QMetaObject::invokeMethod(this, [this, pct](){
                m_dlProgress->setValue(pct);
            }, Qt::QueuedConnection);
        },
        [this, majorVersion](bool ok, const QString& err){
            QMetaObject::invokeMethod(this, [this, ok, err, majorVersion](){
                m_dlProgress->setVisible(false);
                if (ok) {
                    m_dlStatus->setText(QString("✓ Java %1 installed successfully").arg(majorVersion));
                    refreshJavaList();
                } else {
                    m_dlStatus->setText("✗ Failed: " + err);
                }
            }, Qt::QueuedConnection);
        });
}
