#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>

class JavaSettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit JavaSettingsPage(QWidget* parent = nullptr);
private slots:
    void refreshJavaList();
    void onDownloadJava(int majorVersion);
    void onDetect();
private:
    void setupUi();
    QListWidget*  m_javaList    = nullptr;
    QProgressBar* m_dlProgress  = nullptr;
    QLabel*       m_dlStatus    = nullptr;
    QPushButton*  m_detectBtn   = nullptr;
};
