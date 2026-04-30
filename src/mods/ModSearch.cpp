#include "ModSearch.h"
#include "../utils/DownloadManager.h"
#include "../utils/AppConfig.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>

const QString ModSearch::MODRINTH_API   = "https://api.modrinth.com/v2";
const QString ModSearch::CURSEFORGE_API = "https://api.curseforge.com/v1";
// Public CurseForge API key (community key — replace with your own if needed)
const QString ModSearch::CF_API_KEY     = "$2a$10$bL4bIL5pUWqfcO7KwqnNa.hAA5MXFHOM1dHGJDt9fNuFnXlH0n4Vy";

ModSearch::ModSearch(QObject* parent) : QObject(parent) {}
ModSearch& ModSearch::instance() {
    static ModSearch inst;
    return inst;
}

void ModSearch::searchModrinth(const QString& query,
    const QString& mcVersion, const QString& loaderType,
    int offset, int limit,
    std::function<void(bool, QList<ModResult>, int)> cb)
{
    QUrlQuery q;
    q.addQueryItem("query", query);
    q.addQueryItem("limit", QString::number(limit));
    q.addQueryItem("offset", QString::number(offset));
    q.addQueryItem("project_type", "mod");

    // Build facets for version/loader filter
    QStringList facets;
    if (!mcVersion.isEmpty())
        facets << "[\"versions:" + mcVersion + "\"]";
    if (!loaderType.isEmpty())
        facets << "[\"categories:" + loaderType.toLower() + "\"]";
    if (!facets.isEmpty())
        q.addQueryItem("facets", "["  + facets.join(",") + "]");

    QString url = MODRINTH_API + "/search?" + q.toString(QUrl::FullyEncoded);

    DownloadManager::instance().getJson(url,
        [cb](bool ok, const QJsonDocument& doc, const QString& err){
            if (!ok) { cb(false, {}, 0); return; }
            auto obj   = doc.object();
            int total  = obj["total_hits"].toInt();
            QList<ModResult> results;
            for (auto v : obj["hits"].toArray()) {
                auto o = v.toObject();
                ModResult r;
                r.id          = o["project_id"].toString();
                r.slug        = o["slug"].toString();
                r.title       = o["title"].toString();
                r.description = o["description"].toString();
                r.author      = o["author"].toString();
                r.iconUrl     = o["icon_url"].toString();
                r.downloads   = (qint64)o["downloads"].toDouble();
                r.source      = "modrinth";
                r.raw         = o;
                results << r;
            }
            cb(true, results, total);
        });
}

void ModSearch::getModrinthVersions(const QString& modId,
    const QString& mcVersion, const QString& loaderType,
    std::function<void(bool, QList<ModVersion>)> cb)
{
    QUrlQuery q;
    if (!mcVersion.isEmpty())
        q.addQueryItem("game_versions", "[\"" + mcVersion + "\"]");
    if (!loaderType.isEmpty())
        q.addQueryItem("loaders", "[\"" + loaderType.toLower() + "\"]");

    QString url = MODRINTH_API + "/project/" + modId + "/version";
    if (!q.isEmpty()) url += "?" + q.toString(QUrl::FullyEncoded);

    DownloadManager::instance().getJson(url,
        [cb](bool ok, const QJsonDocument& doc, const QString&){
            if (!ok) { cb(false, {}); return; }
            QList<ModVersion> versions;
            for (auto v : doc.array()) {
                auto o = v.toObject();
                ModVersion mv;
                mv.id            = o["id"].toString();
                mv.name          = o["name"].toString();
                mv.versionNumber = o["version_number"].toString();
                mv.datePublished = o["date_published"].toString();
                mv.changelog     = o["changelog"].toString();
                auto mcVers = o["game_versions"].toArray();
                if (!mcVers.isEmpty()) mv.mcVersion = mcVers.last().toString();
                auto loaders = o["loaders"].toArray();
                if (!loaders.isEmpty()) mv.loaderType = loaders[0].toString();

                // First file
                auto files = o["files"].toArray();
                if (!files.isEmpty()) {
                    auto f = files[0].toObject();
                    mv.downloadUrl = f["url"].toString();
                    mv.filename    = f["filename"].toString();
                    mv.fileSize    = (qint64)f["size"].toDouble();
                    mv.sha1        = f["hashes"].toObject()["sha1"].toString();
                }
                versions << mv;
            }
            cb(true, versions);
        });
}

void ModSearch::searchCurseForge(const QString& query,
    const QString& mcVersion, const QString& loaderType,
    int offset, int limit,
    std::function<void(bool, QList<ModResult>, int)> cb)
{
    // gameId=432 = Minecraft, classId=6 = Mods
    QUrlQuery q;
    q.addQueryItem("gameId", "432");
    q.addQueryItem("classId", "6");
    q.addQueryItem("searchFilter", query);
    q.addQueryItem("pageSize", QString::number(limit));
    q.addQueryItem("index", QString::number(offset));
    q.addQueryItem("sortField", "2"); // popularity
    q.addQueryItem("sortOrder", "desc");
    if (!mcVersion.isEmpty()) q.addQueryItem("gameVersion", mcVersion);
    if (loaderType == "forge")  q.addQueryItem("modLoaderType", "1");
    if (loaderType == "fabric") q.addQueryItem("modLoaderType", "4");
    if (loaderType == "quilt")  q.addQueryItem("modLoaderType", "5");

    QString url = CURSEFORGE_API + "/mods/search?" + q.toString(QUrl::FullyEncoded);

    auto* nam = new QNetworkAccessManager;
    QNetworkRequest req(url);
    req.setRawHeader("x-api-key", CF_API_KEY.toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader, "KLauncher/1.0");
    auto* reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, [reply, nam, cb](){
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            cb(false, {}, 0);
            return;
        }
        auto doc   = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        auto obj   = doc.object();
        int total  = obj["pagination"].toObject()["totalCount"].toInt();
        QList<ModResult> results;

        for (auto v : obj["data"].toArray()) {
            auto o = v.toObject();
            ModResult r;
            r.id          = QString::number(o["id"].toInt());
            r.title       = o["name"].toString();
            r.description = o["summary"].toString();
            r.author      = o["authors"].toArray().isEmpty() ? ""
                          : o["authors"].toArray()[0].toObject()["name"].toString();
            r.downloads   = (qint64)o["downloadCount"].toDouble();
            r.iconUrl     = o["logo"].toObject()["thumbnailUrl"].toString();
            r.source      = "curseforge";
            r.raw         = o;
            results << r;
        }
        cb(true, results, total);
    });
}

void ModSearch::downloadMod(const ModVersion& version,
    const QString& modsDir,
    std::function<void(int)> progressCb,
    std::function<void(bool, const QString&)> doneCb)
{
    QString dest = modsDir + "/" + version.filename;
    QDir().mkpath(modsDir);

    DownloadTask t;
    t.url      = version.downloadUrl;
    t.destPath = dest;
    t.sha1     = version.sha1;
    t.id       = "mod-dl-" + version.id;

    connect(&DownloadManager::instance(), &DownloadManager::taskProgress,
        &DownloadManager::instance(),
        [t, progressCb](const QString& id, qint64 recv, qint64 total){
            if (id == t.id && total > 0)
                progressCb((int)(recv * 100 / total));
        }, Qt::UniqueConnection);

    connect(&DownloadManager::instance(), &DownloadManager::taskFinished,
        &DownloadManager::instance(),
        [t, doneCb](const QString& id, bool ok, const QString& err){
            if (id == t.id) doneCb(ok, err);
        }, Qt::SingleShotConnection);

    DownloadManager::instance().enqueue(t);
}

void ModSearch::fetchIcon(const QString& url, std::function<void(QPixmap)> cb) {
    if (url.isEmpty()) { cb({}); return; }
    DownloadManager::instance().getBytes(url,
        [cb](bool ok, const QByteArray& data, const QString&){
            if (!ok) { cb({}); return; }
            QPixmap px;
            px.loadFromData(data);
            cb(px);
        });
}
