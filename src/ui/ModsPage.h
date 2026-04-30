#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QProgressBar>
#include <QTabWidget>
#include "../mods/ModSearch.h"
#include "../minecraft/InstanceManager.h"

class ModsPage : public QWidget {
    Q_OBJECT
public:
    explicit ModsPage(QWidget* parent = nullptr);

private slots:
    void onSearch();
    void onResultSelected(int row);
    void onDownload();
    void onSourceChanged(int idx);
    void onNextPage();
    void onPrevPage();
    void refreshInstalledMods();

private:
    void setupUi();
    void populateInstances();
    void displayResults(const QList<ModResult>& results, int total);
    void showVersionsFor(const ModResult& mod);

    // Search bar
    QLineEdit*   m_searchEdit  = nullptr;
    QComboBox*   m_sourceBox   = nullptr;
    QComboBox*   m_instanceBox = nullptr;
    QComboBox*   m_loaderFilter= nullptr;
    QPushButton* m_searchBtn   = nullptr;

    // Results
    QListWidget* m_resultsList = nullptr;
    QLabel*      m_resultCount = nullptr;
    QPushButton* m_prevBtn     = nullptr;
    QPushButton* m_nextBtn     = nullptr;
    QLabel*      m_pageLabel   = nullptr;

    // Detail panel
    QLabel*      m_modIcon     = nullptr;
    QLabel*      m_modTitle    = nullptr;
    QLabel*      m_modDesc     = nullptr;
    QLabel*      m_modMeta     = nullptr;
    QComboBox*   m_versionBox  = nullptr;
    QPushButton* m_downloadBtn = nullptr;
    QProgressBar* m_dlProgress = nullptr;
    QLabel*      m_dlStatus    = nullptr;

    // Installed tab
    QListWidget* m_installedList = nullptr;
    QPushButton* m_removeModBtn  = nullptr;
    QPushButton* m_openFolderBtn = nullptr;

    QList<ModResult>  m_results;
    QList<ModVersion> m_modVersions;
    int m_currentPage = 0;
    int m_totalResults = 0;
    static const int PAGE_SIZE = 20;
};
