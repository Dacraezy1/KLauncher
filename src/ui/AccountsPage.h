#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include "../auth/MicrosoftAuth.h"

class AccountsPage : public QWidget {
    Q_OBJECT
public:
    explicit AccountsPage(QWidget* parent = nullptr);
private slots:
    void onAddMicrosoft();
    void onAddOffline();
    void onRemoveAccount();
    void onSetActive();
    void refreshList();
private:
    void setupUi();
    QListWidget*  m_list       = nullptr;
    QPushButton*  m_setActiveBtn = nullptr;
    QPushButton*  m_removeBtn  = nullptr;
    QLabel*       m_activeLabel = nullptr;
};
