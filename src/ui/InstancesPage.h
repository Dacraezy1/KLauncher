#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QSplitter>
#include "../minecraft/InstanceManager.h"

class ConsoleWidget;
class InstanceEditDialog;

class InstancesPage : public QWidget {
    Q_OBJECT
public:
    explicit InstancesPage(QWidget* parent = nullptr);

private slots:
    void onInstancesChanged();
    void onCreateClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onDuplicateClicked();
    void onLaunchClicked();
    void onKillClicked();
    void onInstanceSelected(QListWidgetItem* item);
    void onFilterChanged(const QString& text);

private:
    void setupUi();
    void refreshList();
    void updateDetailPanel(const InstanceConfig& inst);
    void installAndLaunch(const InstanceConfig& inst);
    void doActualLaunch(const InstanceConfig& inst);

    QListWidget*  m_list       = nullptr;
    QLineEdit*    m_search     = nullptr;
    QComboBox*    m_groupFilter= nullptr;
    QWidget*      m_detailPanel= nullptr;
    QLabel*       m_instName   = nullptr;
    QLabel*       m_instMeta   = nullptr;
    QLabel*       m_instIcon   = nullptr;
    QPushButton*  m_launchBtn  = nullptr;
    QPushButton*  m_editBtn    = nullptr;
    QPushButton*  m_deleteBtn  = nullptr;
    QPushButton*  m_dupBtn     = nullptr;
    QPushButton*  m_killBtn    = nullptr;
    QProgressBar* m_installProgress = nullptr;
    QLabel*       m_installStatus   = nullptr;
    ConsoleWidget* m_console   = nullptr;
    QSplitter*    m_splitter   = nullptr;

    QString m_selectedId;
};
