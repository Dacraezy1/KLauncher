#pragma once
#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);
signals:
    void themeChanged(const QString& theme);
private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    QComboBox* m_themeBox     = nullptr;
    QCheckBox* m_minimizeTray = nullptr;
    QCheckBox* m_closeToTray  = nullptr;
    QCheckBox* m_showSnaps    = nullptr;
    QComboBox* m_langBox      = nullptr;
    QLineEdit* m_dataDir      = nullptr;
};
