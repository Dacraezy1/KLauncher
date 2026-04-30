#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QPixmap>
#include <QJsonObject>

struct ModResult {
    QString id;
    QString slug;
    QString title;
    QString description;
    QString author;
    QString iconUrl;
    QString downloadUrl;
    QString filename;
    qint64  downloads = 0;
    qint64  fileSize  = 0;
    QString mcVersion;
    QString loaderType;
    QString source; // "modrinth" or "curseforge"
    QJsonObject raw;
};

struct ModVersion {
    QString id;
    QString name;
    QString versionNumber;
    QString mcVersion;
    QString loaderType;
    QString downloadUrl;
    QString filename;
    QString sha1;
    qint64  fileSize  = 0;
    QString datePublished;
    QString changelog;
};

class ModSearch : public QObject {
    Q_OBJECT
public:
    explicit ModSearch(QObject* parent = nullptr);
    static ModSearch& instance();

    // Search Modrinth
    void searchModrinth(const QString& query,
                        const QString& mcVersion,
                        const QString& loaderType,
                        int offset, int limit,
                        std::function<void(bool, QList<ModResult>, int)> cb);

    void getModrinthVersions(const QString& modId,
                             const QString& mcVersion,
                             const QString& loaderType,
                             std::function<void(bool, QList<ModVersion>)> cb);

    // Search CurseForge (requires API key, we use open endpoint)
    void searchCurseForge(const QString& query,
                          const QString& mcVersion,
                          const QString& loaderType,
                          int offset, int limit,
                          std::function<void(bool, QList<ModResult>, int)> cb);

    // Download a mod to instance
    void downloadMod(const ModVersion& version,
                     const QString& modsDir,
                     std::function<void(int)> progressCb,
                     std::function<void(bool, const QString&)> doneCb);

    // Get icon
    void fetchIcon(const QString& url,
                   std::function<void(QPixmap)> cb);

private:
    static const QString MODRINTH_API;
    static const QString CURSEFORGE_API;
    static const QString CF_API_KEY;
};
