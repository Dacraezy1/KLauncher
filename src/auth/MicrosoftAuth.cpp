#include "MicrosoftAuth.h"
#include "../utils/DownloadManager.h"
#include "../utils/AppConfig.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QSettings>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QDebug>
#include <functional>

// Public client ID for a Minecraft-compatible OAuth app
// Users should replace with their own Azure App client ID registered as a public client
const QString MicrosoftAuth::CLIENT_ID    = "00000000402b5328"; // legacy Minecraft client id
const QString MicrosoftAuth::DEVICE_CODE_URL = "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
const QString MicrosoftAuth::TOKEN_URL    = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
const QString MicrosoftAuth::XBL_AUTH_URL  = "https://user.auth.xboxlive.com/user/authenticate";
const QString MicrosoftAuth::XSTS_AUTH_URL = "https://xsts.auth.xboxlive.com/xsts/authorize";
const QString MicrosoftAuth::MC_AUTH_URL   = "https://api.minecraftservices.com/authentication/login_with_xbox";
const QString MicrosoftAuth::MC_PROFILE_URL= "https://api.minecraftservices.com/minecraft/profile";

MicrosoftAuth::MicrosoftAuth(QObject* parent) : QObject(parent) {}

MicrosoftAuth& MicrosoftAuth::instance() {
    static MicrosoftAuth inst;
    return inst;
}

void MicrosoftAuth::startLogin() {
    doDeviceCodeFlow();
}

void MicrosoftAuth::doDeviceCodeFlow() {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(DEVICE_CODE_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id", CLIENT_ID);
    body.addQueryItem("scope", "XboxLive.signin offline_access");

    auto* reply = nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        auto obj = doc.object();

        QString deviceCode = obj["device_code"].toString();
        QString userCode   = obj["user_code"].toString();
        QString verifyUrl  = obj["verification_uri"].toString();
        int interval       = obj["interval"].toInt(5);
        int expiresIn      = obj["expires_in"].toInt(900);

        // Show dialog with user code
        auto* dlg = new QDialog();
        dlg->setWindowTitle("Microsoft Login");
        dlg->setMinimumWidth(420);
        auto* layout = new QVBoxLayout(dlg);

        auto* infoLabel = new QLabel(
            "<h3 style='color:#4fc3f7'>Sign in with Microsoft</h3>"
            "<p>Visit the URL below and enter your code:</p>"
        );
        infoLabel->setTextFormat(Qt::RichText);
        layout->addWidget(infoLabel);

        auto* urlLabel = new QLabel("<a href='" + verifyUrl + "'>" + verifyUrl + "</a>");
        urlLabel->setOpenExternalLinks(true);
        layout->addWidget(urlLabel);

        auto* codeLabel = new QLabel("<b style='font-size:28px;letter-spacing:6px;color:#69f0ae'>"
                                     + userCode + "</b>");
        codeLabel->setAlignment(Qt::AlignCenter);
        codeLabel->setTextFormat(Qt::RichText);
        layout->addWidget(codeLabel);

        auto* btnRow = new QHBoxLayout;
        auto* copyBtn  = new QPushButton("Copy Code");
        auto* openBtn  = new QPushButton("Open Browser");
        auto* cancelBtn= new QPushButton("Cancel");
        btnRow->addWidget(copyBtn);
        btnRow->addWidget(openBtn);
        btnRow->addStretch();
        btnRow->addWidget(cancelBtn);
        layout->addLayout(btnRow);

        connect(copyBtn, &QPushButton::clicked, [userCode](){
            qApp->clipboard()->setText(userCode);
        });
        connect(openBtn, &QPushButton::clicked, [verifyUrl](){
            QDesktopServices::openUrl(QUrl(verifyUrl));
        });
        connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

        dlg->show();

        connect(this, &MicrosoftAuth::loginSuccess, dlg, &QDialog::accept);
        connect(this, &MicrosoftAuth::loginFailed,  dlg, &QDialog::reject);

        pollDeviceToken(deviceCode, interval, expiresIn);
    });
}

void MicrosoftAuth::pollDeviceToken(const QString& deviceCode, int interval, int expires) {
    auto* timer = new QTimer(this);
    int elapsed = 0;
    timer->setInterval(interval * 1000);

    connect(timer, &QTimer::timeout, this, [this, timer, deviceCode, interval, expires, elapsed]() mutable {
        elapsed += interval;
        if (elapsed >= expires) {
            timer->stop();
            timer->deleteLater();
            emit loginFailed("Login timed out");
            return;
        }

        auto* nam = new QNetworkAccessManager(this);
        QNetworkRequest req(QUrl(TOKEN_URL));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QUrlQuery body;
        body.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");
        body.addQueryItem("client_id", CLIENT_ID);
        body.addQueryItem("device_code", deviceCode);

        auto* reply = nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [this, timer, reply, nam]() {
            nam->deleteLater();
            auto doc = QJsonDocument::fromJson(reply->readAll());
            reply->deleteLater();
            auto obj = doc.object();

            if (obj.contains("access_token")) {
                timer->stop();
                timer->deleteLater();
                m_pending.refreshToken = obj["refresh_token"].toString();
                authenticateXBL(obj["access_token"].toString());
            }
            // else: authorization_pending — keep polling
        });
    });
    timer->start();
}

void MicrosoftAuth::authenticateXBL(const QString& msAccessToken) {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(XBL_AUTH_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");

    QJsonObject body {
        {"Properties", QJsonObject{
            {"AuthMethod", "RPS"},
            {"SiteName", "user.auth.xboxlive.com"},
            {"RpsTicket", "d=" + msAccessToken}
        }},
        {"RelyingParty", "http://auth.xboxlive.com"},
        {"TokenType", "JWT"}
    };

    auto* reply = nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("XBL auth failed: " + reply->errorString());
            reply->deleteLater(); return;
        }
        auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        QString xblToken = obj["Token"].toString();
        auto uhs = obj["DisplayClaims"].toObject()["xui"].toArray()[0].toObject()["uhs"].toString();
        m_pending.xuid = uhs;
        authenticateXSTS(xblToken);
    });
}

void MicrosoftAuth::authenticateXSTS(const QString& xblToken) {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(XSTS_AUTH_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");

    QJsonObject body {
        {"Properties", QJsonObject{
            {"SandboxId", "RETAIL"},
            {"UserTokens", QJsonArray{ xblToken }}
        }},
        {"RelyingParty", "rp://api.minecraftservices.com/"},
        {"TokenType", "JWT"}
    };

    auto* reply = nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("XSTS auth failed: " + reply->errorString());
            reply->deleteLater(); return;
        }
        auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        if (obj.contains("XErr")) {
            qint64 xerr = (qint64)obj["XErr"].toDouble();
            QString msg = "XSTS error: " + QString::number(xerr);
            if (xerr == 2148916233) msg = "Microsoft account has no Xbox profile.";
            else if (xerr == 2148916238) msg = "This is a child account. Please use an adult account.";
            emit loginFailed(msg);
            return;
        }

        QString xstsToken = obj["Token"].toString();
        QString userHash  = obj["DisplayClaims"].toObject()["xui"].toArray()[0]
                               .toObject()["uhs"].toString();
        authenticateMinecraft(xstsToken, userHash);
    });
}

void MicrosoftAuth::authenticateMinecraft(const QString& xstsToken, const QString& userHash) {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(MC_AUTH_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body {
        {"identityToken", "XBL3.0 x=" + userHash + ";" + xstsToken}
    };

    auto* reply = nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("MC auth failed: " + reply->errorString());
            reply->deleteLater(); return;
        }
        auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        m_pending.accessToken = obj["access_token"].toString();
        m_pending.expiresAt   = QDateTime::currentSecsSinceEpoch()
                              + obj["expires_in"].toInt(86400);
        fetchProfile(m_pending.accessToken);
    });
}

void MicrosoftAuth::fetchProfile(const QString& mcAccessToken) {
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(MC_PROFILE_URL));
    req.setRawHeader("Authorization", ("Bearer " + mcAccessToken).toUtf8());

    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("Profile fetch failed: " + reply->errorString());
            reply->deleteLater(); return;
        }
        auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();

        m_pending.uuid     = obj["id"].toString();
        m_pending.username = obj["name"].toString();
        m_pending.valid    = true;

        saveAccount(m_pending);
        setActiveAccount(m_pending.uuid);
        emit loginSuccess(m_pending);
    });
}

void MicrosoftAuth::saveAccount(const MicrosoftAccount& acc) {
    QSettings s;
    s.beginGroup("accounts/" + acc.uuid);
    s.setValue("username",     acc.username);
    s.setValue("accessToken",  acc.accessToken);
    s.setValue("refreshToken", acc.refreshToken);
    s.setValue("xuid",         acc.xuid);
    s.setValue("expiresAt",    acc.expiresAt);
    s.setValue("valid",        acc.valid);
    s.endGroup();

    // Add to list
    QStringList uuids = s.value("account_list").toStringList();
    if (!uuids.contains(acc.uuid)) {
        uuids << acc.uuid;
        s.setValue("account_list", uuids);
    }
}

QList<MicrosoftAccount> MicrosoftAuth::loadAccounts() const {
    QSettings s;
    QStringList uuids = s.value("account_list").toStringList();
    QList<MicrosoftAccount> list;
    for (auto& uuid : uuids) {
        s.beginGroup("accounts/" + uuid);
        MicrosoftAccount acc;
        acc.uuid         = uuid;
        acc.username     = s.value("username").toString();
        acc.accessToken  = s.value("accessToken").toString();
        acc.refreshToken = s.value("refreshToken").toString();
        acc.xuid         = s.value("xuid").toString();
        acc.expiresAt    = s.value("expiresAt").toLongLong();
        acc.valid        = s.value("valid").toBool();
        s.endGroup();
        list << acc;
    }
    return list;
}

void MicrosoftAuth::removeAccount(const QString& uuid) {
    QSettings s;
    s.remove("accounts/" + uuid);
    QStringList uuids = s.value("account_list").toStringList();
    uuids.removeAll(uuid);
    s.setValue("account_list", uuids);
}

MicrosoftAccount MicrosoftAuth::activeAccount() const {
    QSettings s;
    QString uuid = s.value("active_account").toString();
    auto accounts = loadAccounts();
    for (auto& a : accounts) if (a.uuid == uuid) return a;
    return {};
}

void MicrosoftAuth::setActiveAccount(const QString& uuid) {
    QSettings s;
    s.setValue("active_account", uuid);
}

void MicrosoftAuth::refreshToken(const MicrosoftAccount& acc,
    std::function<void(bool, MicrosoftAccount)> cb)
{
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(TOKEN_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id", CLIENT_ID);
    body.addQueryItem("refresh_token", acc.refreshToken);
    body.addQueryItem("grant_type", "refresh_token");

    auto* reply = nam->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, acc, cb]() mutable {
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            cb(false, acc);
            return;
        }
        auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        m_pending = acc;
        m_pending.refreshToken = obj["refresh_token"].toString();
        authenticateXBL(obj["access_token"].toString());
        connect(this, &MicrosoftAuth::loginSuccess, this, [cb](const MicrosoftAccount& a){
            cb(true, a);
        }, Qt::SingleShotConnection);
        connect(this, &MicrosoftAuth::loginFailed, this, [cb, acc](const QString&){
            cb(false, acc);
        }, Qt::SingleShotConnection);
    });
}
