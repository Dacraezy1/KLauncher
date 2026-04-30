#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QTabWidget>
#include <QLabel>
#include <QProgressBar>
#include "../minecraft/InstanceManager.h"

class InstanceEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit InstanceEditDialog(const InstanceConfig& cfg, QWidget* parent = nullptr);
    InstanceConfig config() const;

private slots:
    void onMcVersionChanged(const QString& ver);
    void onModLoaderChanged(int index);
    void onFetchLoaderVersions();
    void onBrowseJava();
    void onBrowseIcon();
    void onRamChanged();

private:
    void setupUi();
    void populateVersions();
    void populateLoaderVersions(const QString& mcVer, int loaderIdx);

    InstanceConfig m_cfg;
    bool           m_editMode;

    // General tab
    QLineEdit*  m_nameEdit     = nullptr;
    QLabel*     m_iconLabel    = nullptr;
    QString     m_iconPath;
    QComboBox*  m_mcVersionBox = nullptr;
    QCheckBox*  m_showSnaps    = nullptr;
    QComboBox*  m_loaderBox    = nullptr;
    QComboBox*  m_loaderVerBox = nullptr;
    QLabel*     m_loaderStatus = nullptr;
    QLineEdit*  m_groupEdit    = nullptr;
    QTextEdit*  m_notesEdit    = nullptr;

    // Java tab
    QLineEdit*  m_javaPath     = nullptr;
    QLabel*     m_javaDetected = nullptr;
    QSpinBox*   m_minRam       = nullptr;
    QSpinBox*   m_maxRam       = nullptr;
    QLabel*     m_ramPreview   = nullptr;
    QTextEdit*  m_jvmArgs      = nullptr;

    // Window tab
    QLineEdit*  m_resolution   = nullptr;
    QCheckBox*  m_fullscreen   = nullptr;
    QLineEdit*  m_gameDir      = nullptr;

    QProgressBar* m_loaderProgress = nullptr;
};
