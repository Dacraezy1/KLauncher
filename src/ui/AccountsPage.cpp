#include "AccountsPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QFrame>

AccountsPage::AccountsPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshList();

    connect(&MicrosoftAuth::instance(), &MicrosoftAuth::loginSuccess,
            this, [this](const MicrosoftAccount&){ refreshList(); });
    connect(&MicrosoftAuth::instance(), &MicrosoftAuth::loginFailed,
            this, [this](const QString& err){
                QMessageBox::warning(this, "Login Failed", err);
            });
}

void AccountsPage::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(20);

    auto* title = new QLabel("Accounts");
    title->setObjectName("pageTitle");
    root->addWidget(title);

    // Active account banner
    m_activeLabel = new QLabel("No active account");
    m_activeLabel->setStyleSheet(
        "background: rgba(79,195,247,0.10); border: 1px solid rgba(79,195,247,0.3);"
        "border-radius: 8px; color: #4fc3f7; font-size: 13px; padding: 10px 16px;");
    root->addWidget(m_activeLabel);

    // Account list
    auto* listGroup = new QGroupBox("Saved Accounts");
    auto* listLayout = new QVBoxLayout(listGroup);

    m_list = new QListWidget;
    m_list->setMinimumHeight(200);
    m_list->setIconSize(QSize(36, 36));
    m_list->setSpacing(2);
    listLayout->addWidget(m_list);

    auto* btnRow = new QHBoxLayout;
    m_setActiveBtn = new QPushButton("✓  Set Active");
    m_setActiveBtn->setFixedHeight(34);
    m_setActiveBtn->setEnabled(false);
    connect(m_setActiveBtn, &QPushButton::clicked, this, &AccountsPage::onSetActive);
    btnRow->addWidget(m_setActiveBtn);

    m_removeBtn = new QPushButton("🗑  Remove");
    m_removeBtn->setObjectName("dangerBtn");
    m_removeBtn->setFixedHeight(34);
    m_removeBtn->setEnabled(false);
    connect(m_removeBtn, &QPushButton::clicked, this, &AccountsPage::onRemoveAccount);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    listLayout->addLayout(btnRow);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row){
        bool sel = row >= 0;
        m_setActiveBtn->setEnabled(sel);
        m_removeBtn->setEnabled(sel);
    });
    root->addWidget(listGroup);

    // Add account group
    auto* addGroup = new QGroupBox("Add Account");
    auto* addLayout = new QVBoxLayout(addGroup);
    addLayout->setSpacing(10);

    // Microsoft button
    auto* msBtn = new QPushButton("  Sign in with Microsoft");
    msBtn->setFixedHeight(44);
    msBtn->setStyleSheet(
        "QPushButton { background: #0078d4; color: white; font-size: 14px; font-weight: 600;"
        "border: none; border-radius: 8px; padding: 0 20px; }"
        "QPushButton:hover { background: #1a88e4; }"
        "QPushButton:pressed { background: #006bbf; }");
    connect(msBtn, &QPushButton::clicked, this, &AccountsPage::onAddMicrosoft);
    addLayout->addWidget(msBtn);

    auto* msHint = new QLabel(
        "Sign in with your Microsoft/Xbox account to play online. "
        "Uses a secure device code flow — no password is stored in KLauncher.");
    msHint->setWordWrap(true);
    msHint->setStyleSheet("color: #607d8b; font-size: 11px;");
    addLayout->addWidget(msHint);

    // Separator
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #2d4a7a;");
    addLayout->addWidget(sep);

    // Offline button
    auto* offBtn = new QPushButton("🕹  Add Offline Account");
    offBtn->setFixedHeight(38);
    connect(offBtn, &QPushButton::clicked, this, &AccountsPage::onAddOffline);
    addLayout->addWidget(offBtn);

    auto* offHint = new QLabel(
        "Offline accounts use a local username only. You can play on offline-mode servers "
        "and single player, but NOT on online-mode servers.");
    offHint->setWordWrap(true);
    offHint->setStyleSheet("color: #607d8b; font-size: 11px;");
    addLayout->addWidget(offHint);
    root->addWidget(addGroup);

    root->addStretch();
}

void AccountsPage::refreshList() {
    m_list->clear();
    auto accounts = MicrosoftAuth::instance().loadAccounts();
    auto active   = MicrosoftAuth::instance().activeAccount();

    for (auto& acc : accounts) {
        auto* item = new QListWidgetItem;
        bool isActive = acc.uuid == active.uuid;
        QString prefix = isActive ? "✓  " : "   ";
        QString type   = acc.accessToken.isEmpty() ? "[Offline]" : "[Microsoft]";
        item->setText(prefix + acc.username + "  " + type);
        item->setData(Qt::UserRole, acc.uuid);
        if (isActive) item->setForeground(QColor("#4fc3f7"));
        m_list->addItem(item);
    }

    // Update banner
    if (active.valid || !active.username.isEmpty()) {
        QString type = active.accessToken.isEmpty() ? "Offline" : "Microsoft";
        m_activeLabel->setText("✓  Active: " + active.username + " (" + type + ")");
    } else {
        m_activeLabel->setText("No active account — you will be prompted for offline name at launch");
    }
}

void AccountsPage::onAddMicrosoft() {
    MicrosoftAuth::instance().startLogin();
}

void AccountsPage::onAddOffline() {
    bool ok;
    QString name = QInputDialog::getText(this, "Add Offline Account",
        "Enter username:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    MicrosoftAccount acc;
    acc.uuid     = "offline-" + name.trimmed().toLower().replace(' ', '-');
    acc.username = name.trimmed();
    acc.valid    = true;
    // No accessToken = offline

    MicrosoftAuth::instance().saveAccount(acc);
    MicrosoftAuth::instance().setActiveAccount(acc.uuid);
    refreshList();
}

void AccountsPage::onRemoveAccount() {
    auto* item = m_list->currentItem();
    if (!item) return;
    QString uuid = item->data(Qt::UserRole).toString();
    auto res = QMessageBox::question(this, "Remove Account",
        "Remove account \"" + item->text().trimmed() + "\"?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::Yes) {
        MicrosoftAuth::instance().removeAccount(uuid);
        refreshList();
    }
}

void AccountsPage::onSetActive() {
    auto* item = m_list->currentItem();
    if (!item) return;
    MicrosoftAuth::instance().setActiveAccount(item->data(Qt::UserRole).toString());
    refreshList();
}
