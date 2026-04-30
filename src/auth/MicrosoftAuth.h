#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>

struct MicrosoftAccount {
    QString uuid;
    QString username;
    QString accessToken;
    QString refreshToken;
    QString xuid;
    QString clientToken;
    qint64  expiresAt = 0;   // Unix timestamp
    bool    valid     = false;
};

class MicrosoftAuth : public QObject {
    Q_OBJECT
public:
    explicit MicrosoftAuth(QObject* parent = nullptr);
    ~MicrosoftAuth() override = default;

    static MicrosoftAuth& instance();

    // Opens OAuth2 device-code flow in a QDialog with embedded browser
    void startLogin();

    // Refresh existing token
    void refreshToken(const MicrosoftAccount& acc,
                      std::function<void(bool, MicrosoftAccount)> cb);

    // Save/load accounts from encrypted settings
    void saveAccount(const MicrosoftAccount& acc);
    QList<MicrosoftAccount> loadAccounts() const;
    void removeAccount(const QString& uuid);

    MicrosoftAccount activeAccount() const;
    void setActiveAccount(const QString& uuid);

signals:
    void loginSuccess(const MicrosoftAccount& account);
    void loginFailed(const QString& error);
    void tokenRefreshed(const MicrosoftAccount& account);

private:
    // Microsoft OAuth endpoints
    static const QString CLIENT_ID;
    static const QString DEVICE_CODE_URL;
    static const QString TOKEN_URL;
    static const QString XBL_AUTH_URL;
    static const QString XSTS_AUTH_URL;
    static const QString MC_AUTH_URL;
    static const QString MC_PROFILE_URL;

    void doDeviceCodeFlow();
    void pollDeviceToken(const QString& deviceCode, int interval, int expires);
    void authenticateXBL(const QString& msAccessToken);
    void authenticateXSTS(const QString& xblToken);
    void authenticateMinecraft(const QString& xstsToken, const QString& userHash);
    void fetchProfile(const QString& mcAccessToken);

    MicrosoftAccount m_pending;
    QString          m_activeUuid;
};
