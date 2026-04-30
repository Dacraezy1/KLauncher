#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QSystemTrayIcon>

class InstancesPage;
class AccountsPage;
class ModsPage;
class JavaSettingsPage;
class SettingsPage;
class NewsPage;
class ConsoleWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private slots:
    void onNavClicked(int index);
    void onThemeChanged(const QString& theme);

private:
    void setupUi();
    void setupSidebar();
    void setupPages();
    void setupStatusBar();
    void setupTray();
    void applyTheme(const QString& name);
    void loadTheme(const QString& qssPath);

    QWidget*       m_sidebar   = nullptr;
    QStackedWidget* m_pages    = nullptr;
    QLabel*        m_logoLabel = nullptr;
    QList<QPushButton*> m_navBtns;

    // Pages
    InstancesPage*    m_instancesPage  = nullptr;
    AccountsPage*     m_accountsPage   = nullptr;
    ModsPage*         m_modsPage       = nullptr;
    JavaSettingsPage* m_javaPage       = nullptr;
    SettingsPage*     m_settingsPage   = nullptr;

    // Status bar widgets
    QLabel*      m_statusLabel    = nullptr;
    QProgressBar* m_globalProgress = nullptr;

    QSystemTrayIcon* m_tray = nullptr;
    int m_currentPage = 0;
};
