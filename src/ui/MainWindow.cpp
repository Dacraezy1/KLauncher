#include "MainWindow.h"
#include "InstancesPage.h"
#include "AccountsPage.h"
#include "ModsPage.h"
#include "JavaSettingsPage.h"
#include "SettingsPage.h"
#include "../utils/AppConfig.h"
#include "../utils/DownloadManager.h"
#include "../minecraft/GameLauncher.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QStatusBar>
#include <QFrame>
#include <QCloseEvent>
#include <QFile>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("KLauncher");
    setWindowIcon(QIcon(":/icons/klauncher.png"));
    setMinimumSize(1000, 650);
    resize(1200, 720);

    setupUi();
    setupTray();

    // Apply saved theme
    QString theme = AppConfig::instance().getString("theme", "dark");
    applyTheme(theme);

    // Connect global download progress
    connect(&DownloadManager::instance(), &DownloadManager::taskProgress,
        this, [this](const QString&, qint64 recv, qint64 total){
            if (total > 0 && m_globalProgress) {
                m_globalProgress->setVisible(true);
                m_globalProgress->setValue((int)(recv * 100 / total));
            }
        });
    connect(&DownloadManager::instance(), &DownloadManager::allFinished,
        this, [this](){
            if (m_globalProgress) {
                m_globalProgress->setValue(100);
                QTimer::singleShot(1200, m_globalProgress, [this](){
                    m_globalProgress->setVisible(false);
                    m_globalProgress->setValue(0);
                });
            }
        });
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    setupSidebar();
    rootLayout->addWidget(m_sidebar);

    // Separator line
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setObjectName("sidebarSep");
    sep->setFixedWidth(1);
    rootLayout->addWidget(sep);

    setupPages();
    rootLayout->addWidget(m_pages, 1);

    setupStatusBar();
}

void MainWindow::setupSidebar() {
    m_sidebar = new QWidget;
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(200);

    auto* layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Logo area
    auto* logoArea = new QWidget;
    logoArea->setObjectName("logoArea");
    logoArea->setFixedHeight(80);
    auto* logoLayout = new QVBoxLayout(logoArea);
    logoLayout->setContentsMargins(16, 12, 16, 12);

    m_logoLabel = new QLabel;
    m_logoLabel->setObjectName("logoLabel");
    m_logoLabel->setText("<span style='font-size:22px;font-weight:800;letter-spacing:2px;'>K</span>"
                         "<span style='font-size:16px;font-weight:600;'>Launcher</span>");
    m_logoLabel->setTextFormat(Qt::RichText);
    logoLayout->addWidget(m_logoLabel, 0, Qt::AlignCenter);
    layout->addWidget(logoArea);

    // Nav buttons
    struct NavItem { QString icon; QString label; };
    QList<NavItem> items = {
        {"🎮", "Instances"},
        {"👤", "Accounts"},
        {"🧩", "Mods"},
        {"☕", "Java"},
        {"⚙️", "Settings"},
    };

    layout->addSpacing(8);
    for (int i = 0; i < items.size(); ++i) {
        auto* btn = new QPushButton(items[i].icon + "  " + items[i].label);
        btn->setObjectName("navBtn");
        btn->setCheckable(true);
        btn->setFlat(true);
        btn->setFixedHeight(44);
        btn->setProperty("navIndex", i);
        connect(btn, &QPushButton::clicked, this, [this, i](){ onNavClicked(i); });
        m_navBtns << btn;
        layout->addWidget(btn);
    }

    // Set first active
    if (!m_navBtns.isEmpty()) m_navBtns[0]->setChecked(true);

    layout->addStretch();

    // Version label at bottom
    auto* verLabel = new QLabel("v" + QString(KLAUNCHER_VERSION));
    verLabel->setObjectName("versionLabel");
    verLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(verLabel);
    layout->addSpacing(8);
}

void MainWindow::setupPages() {
    m_pages = new QStackedWidget;

    m_instancesPage = new InstancesPage;
    m_accountsPage  = new AccountsPage;
    m_modsPage      = new ModsPage;
    m_javaPage      = new JavaSettingsPage;
    m_settingsPage  = new SettingsPage;

    m_pages->addWidget(m_instancesPage);   // 0
    m_pages->addWidget(m_accountsPage);    // 1
    m_pages->addWidget(m_modsPage);        // 2
    m_pages->addWidget(m_javaPage);        // 3
    m_pages->addWidget(m_settingsPage);    // 4

    connect(m_settingsPage, &SettingsPage::themeChanged,
            this, &MainWindow::onThemeChanged);
}

void MainWindow::setupStatusBar() {
    auto* bar = statusBar();
    bar->setObjectName("mainStatusBar");

    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setObjectName("statusLabel");
    bar->addWidget(m_statusLabel, 1);

    m_globalProgress = new QProgressBar;
    m_globalProgress->setObjectName("globalProgress");
    m_globalProgress->setFixedWidth(180);
    m_globalProgress->setFixedHeight(14);
    m_globalProgress->setVisible(false);
    m_globalProgress->setTextVisible(false);
    bar->addPermanentWidget(m_globalProgress);
}

void MainWindow::setupTray() {
    m_tray = new QSystemTrayIcon(QIcon(":/icons/klauncher.png"), this);
    auto* menu = new QMenu;
    auto* showAct = menu->addAction("Show KLauncher");
    connect(showAct, &QAction::triggered, this, &QWidget::show);
    auto* quitAct = menu->addAction("Quit");
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason r){
            if (r == QSystemTrayIcon::Trigger) {
                isVisible() ? hide() : show();
            }
        });
}

void MainWindow::onNavClicked(int index) {
    m_currentPage = index;
    for (int i = 0; i < m_navBtns.size(); ++i)
        m_navBtns[i]->setChecked(i == index);
    m_pages->setCurrentIndex(index);
}

void MainWindow::onThemeChanged(const QString& theme) {
    applyTheme(theme);
    AppConfig::instance().set("theme", theme);
}

void MainWindow::applyTheme(const QString& name) {
    QString path = ":/themes/" + name + ".qss";
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    } else {
        // Fallback: embedded dark theme
        qApp->setStyleSheet(R"(
QMainWindow, QWidget { background: #1a1a2e; color: #e0e0e0; font-family: 'Segoe UI', Ubuntu, sans-serif; font-size: 13px; }
#sidebar { background: #16213e; }
#logoArea { background: #0f3460; }
#logoLabel { color: #4fc3f7; }
#navBtn { text-align: left; padding: 0 20px; border: none; border-radius: 0; color: #b0b8c8; font-size: 13px; background: transparent; }
#navBtn:hover { background: rgba(79,195,247,0.08); color: #e0e0e0; }
#navBtn:checked { background: rgba(79,195,247,0.15); color: #4fc3f7; border-left: 3px solid #4fc3f7; }
#sidebarSep { color: #2a2a4a; }
#versionLabel { color: #4a5568; font-size: 11px; }
QStackedWidget { background: #1a1a2e; }
QPushButton { background: #0f3460; color: #e0e0e0; border: 1px solid #2d4a7a; border-radius: 6px; padding: 7px 16px; font-size: 13px; }
QPushButton:hover { background: #1a4a7a; border-color: #4fc3f7; }
QPushButton:pressed { background: #0a2a50; }
QPushButton:disabled { background: #1e1e3e; color: #4a5568; border-color: #2a2a4a; }
QPushButton#launchBtn { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #00b4db,stop:1 #0083b0); color: white; font-weight: bold; font-size: 14px; border: none; border-radius: 8px; padding: 10px 24px; }
QPushButton#launchBtn:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #00c9ef,stop:1 #0099cc); }
QPushButton#dangerBtn { background: #7f1d1d; border-color: #b91c1c; }
QPushButton#dangerBtn:hover { background: #991b1b; border-color: #ef4444; }
QLineEdit, QTextEdit, QPlainTextEdit { background: #0f1b2d; border: 1px solid #2d4a7a; border-radius: 6px; color: #e0e0e0; padding: 6px 10px; selection-background-color: #0f3460; }
QLineEdit:focus, QTextEdit:focus { border-color: #4fc3f7; }
QComboBox { background: #0f1b2d; border: 1px solid #2d4a7a; border-radius: 6px; color: #e0e0e0; padding: 6px 10px; }
QComboBox:hover { border-color: #4fc3f7; }
QComboBox QAbstractItemView { background: #0f1b2d; border: 1px solid #2d4a7a; color: #e0e0e0; selection-background-color: #0f3460; }
QSpinBox { background: #0f1b2d; border: 1px solid #2d4a7a; border-radius: 6px; color: #e0e0e0; padding: 6px 10px; }
QSpinBox:hover { border-color: #4fc3f7; }
QSlider::groove:horizontal { background: #2d4a7a; height: 4px; border-radius: 2px; }
QSlider::handle:horizontal { background: #4fc3f7; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }
QSlider::sub-page:horizontal { background: #4fc3f7; border-radius: 2px; }
QProgressBar { background: #0f1b2d; border: 1px solid #2d4a7a; border-radius: 6px; height: 8px; }
QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #00b4db,stop:1 #4fc3f7); border-radius: 6px; }
QScrollBar:vertical { background: #0f1b2d; width: 8px; border-radius: 4px; }
QScrollBar::handle:vertical { background: #2d4a7a; border-radius: 4px; min-height: 20px; }
QScrollBar::handle:vertical:hover { background: #4fc3f7; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: #0f1b2d; height: 8px; }
QScrollBar::handle:horizontal { background: #2d4a7a; border-radius: 4px; min-width: 20px; }
QLabel { color: #e0e0e0; }
QLabel#pageTitle { font-size: 20px; font-weight: 700; color: #4fc3f7; }
QLabel#sectionTitle { font-size: 14px; font-weight: 600; color: #90caf9; }
QGroupBox { border: 1px solid #2d4a7a; border-radius: 8px; margin-top: 12px; padding-top: 8px; color: #90caf9; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #4fc3f7; }
QTabWidget::pane { border: 1px solid #2d4a7a; border-radius: 6px; background: #1a1a2e; }
QTabBar::tab { background: #16213e; color: #b0b8c8; border: 1px solid #2d4a7a; border-bottom: none; border-radius: 6px 6px 0 0; padding: 8px 18px; margin-right: 2px; }
QTabBar::tab:selected { background: #0f3460; color: #4fc3f7; border-color: #4fc3f7; }
QTabBar::tab:hover { background: #1a3a60; color: #e0e0e0; }
QTableWidget, QListWidget, QTreeWidget { background: #0f1b2d; border: 1px solid #2d4a7a; border-radius: 6px; color: #e0e0e0; gridline-color: #2d4a7a; selection-background-color: #0f3460; }
QTableWidget::item:selected, QListWidget::item:selected { background: #0f3460; color: #4fc3f7; }
QHeaderView::section { background: #16213e; color: #90caf9; border: none; border-right: 1px solid #2d4a7a; padding: 6px 10px; font-weight: 600; }
QCheckBox { color: #e0e0e0; spacing: 8px; }
QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid #2d4a7a; border-radius: 4px; background: #0f1b2d; }
QCheckBox::indicator:checked { background: #4fc3f7; border-color: #4fc3f7; }
QCheckBox::indicator:hover { border-color: #4fc3f7; }
QStatusBar { background: #16213e; color: #b0b8c8; border-top: 1px solid #2d4a7a; }
QToolTip { background: #0f3460; color: #e0e0e0; border: 1px solid #4fc3f7; border-radius: 4px; padding: 4px 8px; }
QMenu { background: #16213e; border: 1px solid #2d4a7a; border-radius: 6px; color: #e0e0e0; }
QMenu::item:selected { background: #0f3460; color: #4fc3f7; }
QDialog { background: #1a1a2e; color: #e0e0e0; }
QSplitter::handle { background: #2d4a7a; }
)");
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    bool minimizeToTray = AppConfig::instance().getBool("minimizeToTray", true);
    if (minimizeToTray && m_tray->isVisible()) {
        hide();
        e->ignore();
        m_tray->showMessage("KLauncher", "Running in background", QSystemTrayIcon::Information, 2000);
    } else {
        e->accept();
    }
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
}
