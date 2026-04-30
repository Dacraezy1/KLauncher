#include "VersionFetcher.h"
#include "../utils/DownloadManager.h"
#include "../utils/AppConfig.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QQueue>
#include <QAtomicInt>
#include <functional>

const QString VersionFetcher::MANIFEST_URL =
    "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json";

VersionFetcher::VersionFetcher(QObject* parent) : QObject(parent) {}

VersionFetcher& VersionFetcher::instance() {
    static VersionFetcher inst;
    return inst;
}

QList<MCVersion> VersionFetcher::releases() const {
    QList<MCVersion> r;
    for (auto& v : m_versions) if (v.type == "release") r << v;
    return r;
}
QList<MCVersion> VersionFetcher::snapshots() const {
    QList<MCVersion> r;
    for (auto& v : m_versions) if (v.type == "snapshot") r << v;
    return r;
}

bool VersionFetcher::isInstalled(const QString& versionId) const {
    return QFile::exists(versionJarPath(versionId));
}

QString VersionFetcher::versionJsonPath(const QString& id) const {
    return AppConfig::instance().versionsDir() + "/" + id + "/" + id + ".json";
}
QString VersionFetcher::versionJarPath(const QString& id) const {
    return AppConfig::instance().versionsDir() + "/" + id + "/" + id + ".jar";
}

void VersionFetcher::fetchVersionList(std::function<void(bool)> cb) {
    DownloadManager::instance().getJson(MANIFEST_URL, [this, cb](bool ok, const QJsonDocument& doc, const QString& err){
        if (!ok) { if (cb) cb(false); return; }
        auto arr = doc.object()["versions"].toArray();
        m_versions.clear();
        for (auto v : arr) {
            auto o = v.toObject();
            MCVersion mv;
            mv.id          = o["id"].toString();
            mv.type        = o["type"].toString();
            mv.url         = o["url"].toString();
            mv.releaseTime = o["releaseTime"].toString();
            mv.installed   = isInstalled(mv.id);
            m_versions << mv;
        }
        emit versionsLoaded();
        if (cb) cb(true);
    });
}

void VersionFetcher::downloadVersion(const QString& versionId,
    std::function<void(bool, const QString&)> cb)
{
    // First find the version meta URL
    MCVersion found;
    for (auto& v : m_versions) if (v.id == versionId) { found = v; break; }
    if (found.id.isEmpty()) { cb(false, "Version not found: " + versionId); return; }

    QString jsonPath = versionJsonPath(versionId);
    QDir().mkpath(QFileInfo(jsonPath).absolutePath());

    // Step 1: Download version JSON
    DownloadManager::instance().downloadFile(found.url, jsonPath,
        [this, versionId, jsonPath, cb](bool ok, const QString& err) {
            if (!ok) { cb(false, err); return; }

            auto vJson = loadVersionJson(versionId);
            if (vJson.isEmpty()) { cb(false, "Bad version JSON"); return; }

            // Step 2: Download client JAR
            auto clientUrl  = vJson["downloads"].toObject()["client"].toObject()["url"].toString();
            auto clientSha1 = vJson["downloads"].toObject()["client"].toObject()["sha1"].toString();
            QString jarPath = versionJarPath(versionId);

            DownloadTask t;
            t.url      = clientUrl;
            t.destPath = jarPath;
            t.sha1     = clientSha1;
            t.id       = "jar-" + versionId;

            connect(&DownloadManager::instance(), &DownloadManager::taskFinished,
                this, [this, versionId, vJson, cb](const QString& id, bool ok2, const QString& err2){
                    if (id != "jar-" + versionId) return;
                    if (!ok2) { cb(false, err2); return; }

                    // Step 3: Download libraries
                    int total = 0, done = 0;
                    downloadLibraries(vJson,
                        [&total, &done](int d, int t2){ total=t2; done=d; },
                        [this, versionId, vJson, cb](bool libOk){
                            if (!libOk) { cb(false, "Libraries download failed"); return; }
                            // Step 4: Download assets
                            downloadAssets(vJson,
                                [](int,int){},
                                [cb](bool assetsOk){
                                    cb(assetsOk, assetsOk ? "" : "Assets download failed");
                                });
                        });
                }, Qt::SingleShotConnection);

            DownloadManager::instance().enqueue(t);
        });
}

QJsonObject VersionFetcher::loadVersionJson(const QString& versionId) const {
    QFile f(versionJsonPath(versionId));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void VersionFetcher::downloadLibraries(const QJsonObject& versionJson,
    std::function<void(int,int)> progressCb,
    std::function<void(bool)> doneCb)
{
    auto libs = versionJson["libraries"].toArray();
    struct Ctx { QAtomicInt done; int total; bool anyFail; };
    auto ctx = std::make_shared<Ctx>();
    ctx->total = 0; ctx->anyFail = false;

    QList<DownloadTask> tasks;
    for (auto lv : libs) {
        auto lo = lv.toObject();
        // Check rules (os check)
        bool allowed = true;
        if (lo.contains("rules")) {
            allowed = false;
            for (auto rv : lo["rules"].toArray()) {
                auto ro = rv.toObject();
                QString action = ro["action"].toString();
                if (ro.contains("os")) {
                    QString os = ro["os"].toObject()["name"].toString();
                    if (os == "linux" && action == "allow") allowed = true;
                    if (os != "linux" && action == "disallow") allowed = true;
                } else {
                    if (action == "allow") allowed = true;
                }
            }
        }
        if (!allowed) continue;

        auto downloads = lo["downloads"].toObject();
        auto artifact  = downloads["artifact"].toObject();
        if (artifact.isEmpty()) continue;

        QString path = artifact["path"].toString();
        QString url  = artifact["url"].toString();
        QString sha1 = artifact["sha1"].toString();
        if (path.isEmpty() || url.isEmpty()) continue;

        QString dest = AppConfig::instance().librariesDir() + "/" + path;
        if (QFile::exists(dest)) continue;  // already have it

        DownloadTask t;
        t.url      = url;
        t.destPath = dest;
        t.sha1     = sha1;
        t.id       = "lib-" + path;
        tasks << t;
        ctx->total++;
    }

    if (tasks.isEmpty()) { doneCb(true); return; }

    for (auto& t : tasks) {
        connect(&DownloadManager::instance(), &DownloadManager::taskFinished, this,
            [ctx, progressCb, doneCb](const QString& id, bool ok, const QString&){
                if (!id.startsWith("lib-")) return;
                if (!ok) ctx->anyFail = true;
                int d = ctx->done.fetchAndAddRelaxed(1) + 1;
                progressCb(d, ctx->total);
                if (d >= ctx->total) doneCb(!ctx->anyFail);
            }, Qt::UniqueConnection);
        DownloadManager::instance().enqueue(t);
    }
}

void VersionFetcher::downloadAssets(const QJsonObject& versionJson,
    std::function<void(int,int)> progressCb,
    std::function<void(bool)> doneCb)
{
    auto assetIdx = versionJson["assetIndex"].toObject();
    QString indexId  = assetIdx["id"].toString();
    QString indexUrl = assetIdx["url"].toString();
    QString indexPath= AppConfig::instance().assetsDir() + "/indexes/" + indexId + ".json";
    QDir().mkpath(QFileInfo(indexPath).absolutePath());

    DownloadManager::instance().downloadFile(indexUrl, indexPath,
        [this, indexPath, progressCb, doneCb](bool ok, const QString&){
            if (!ok) { doneCb(false); return; }
            QFile f(indexPath);
            if (!f.open(QIODevice::ReadOnly)) { doneCb(false); return; }
            auto objects = QJsonDocument::fromJson(f.readAll()).object()["objects"].toObject();

            struct Ctx { QAtomicInt done; int total; };
            auto ctx = std::make_shared<Ctx>();
            ctx->total = 0;
            QList<DownloadTask> tasks;

            for (auto key : objects.keys()) {
                auto obj  = objects[key].toObject();
                QString hash = obj["hash"].toString();
                QString sub  = hash.left(2);
                QString dest = AppConfig::instance().assetsDir() + "/objects/" + sub + "/" + hash;
                if (QFile::exists(dest)) continue;
                QString url  = "https://resources.download.minecraft.net/" + sub + "/" + hash;
                DownloadTask t;
                t.url      = url;
                t.destPath = dest;
                t.sha1     = hash;
                t.id       = "asset-" + hash;
                tasks << t;
                ctx->total++;
            }

            if (tasks.isEmpty()) { doneCb(true); return; }

            for (auto& t : tasks) {
                connect(&DownloadManager::instance(), &DownloadManager::taskFinished, this,
                    [ctx, progressCb, doneCb](const QString& id, bool ok2, const QString&){
                        if (!id.startsWith("asset-")) return;
                        int d = ctx->done.fetchAndAddRelaxed(1) + 1;
                        progressCb(d, ctx->total);
                        if (d >= ctx->total) doneCb(true);
                    }, Qt::UniqueConnection);
                DownloadManager::instance().enqueue(t);
            }
        });
}
