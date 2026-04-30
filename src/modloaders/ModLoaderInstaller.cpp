#include "ModLoaderInstaller.h"
#include "../utils/DownloadManager.h"
#include "../utils/AppConfig.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <QAtomicInt>
#include <memory>

const QString ModLoaderInstaller::FABRIC_META  = "https://meta.fabricmc.net/v2";
const QString ModLoaderInstaller::QUILT_META   = "https://meta.quiltmc.org/v3";
const QString ModLoaderInstaller::FORGE_MAVEN  = "https://maven.minecraftforge.net";
const QString ModLoaderInstaller::NEOFORGE_MAVEN = "https://maven.neoforged.net/releases";

ModLoaderInstaller::ModLoaderInstaller(QObject* parent) : QObject(parent) {}
ModLoaderInstaller& ModLoaderInstaller::instance() {
    static ModLoaderInstaller inst;
    return inst;
}

void ModLoaderInstaller::fetchFabricVersions(const QString& mcVersion,
    std::function<void(bool, QList<ModLoaderVersion>)> cb)
{
    QString url = FABRIC_META + "/versions/loader/" + mcVersion;
    DownloadManager::instance().getJson(url,
        [mcVersion, cb](bool ok, const QJsonDocument& doc, const QString&){
            if (!ok) { cb(false, {}); return; }
            QList<ModLoaderVersion> list;
            for (auto v : doc.array()) {
                auto o = v.toObject();
                ModLoaderVersion mv;
                mv.mcVersion     = mcVersion;
                mv.loaderVersion = o["loader"].toObject()["version"].toString();
                mv.stable        = o["loader"].toObject()["stable"].toBool(true);
                mv.id            = mv.loaderVersion;
                list << mv;
            }
            cb(true, list);
        });
}

void ModLoaderInstaller::fetchQuiltVersions(const QString& mcVersion,
    std::function<void(bool, QList<ModLoaderVersion>)> cb)
{
    QString url = QUILT_META + "/versions/loader/" + mcVersion;
    DownloadManager::instance().getJson(url,
        [mcVersion, cb](bool ok, const QJsonDocument& doc, const QString&){
            if (!ok) { cb(false, {}); return; }
            QList<ModLoaderVersion> list;
            for (auto v : doc.array()) {
                auto o = v.toObject();
                ModLoaderVersion mv;
                mv.mcVersion     = mcVersion;
                mv.loaderVersion = o["loader"].toObject()["version"].toString();
                mv.stable        = true;
                mv.id            = mv.loaderVersion;
                list << mv;
            }
            cb(true, list);
        });
}

void ModLoaderInstaller::fetchForgeVersions(const QString& mcVersion,
    std::function<void(bool, QList<ModLoaderVersion>)> cb)
{
    // Use BMCLAPI mirror for forge version list (reliable)
    QString url = "https://bmclapi2.bangbang93.com/forge/minecraft/" + mcVersion;
    DownloadManager::instance().getJson(url,
        [mcVersion, cb](bool ok, const QJsonDocument& doc, const QString&){
            if (!ok) { cb(false, {}); return; }
            QList<ModLoaderVersion> list;
            for (auto v : doc.array()) {
                auto o = v.toObject();
                ModLoaderVersion mv;
                mv.mcVersion     = mcVersion;
                mv.loaderVersion = o["version"].toString();
                mv.id            = mv.loaderVersion;
                mv.stable        = true;
                list << mv;
            }
            cb(true, list);
        });
}

void ModLoaderInstaller::fetchNeoForgeVersions(const QString& mcVersion,
    std::function<void(bool, QList<ModLoaderVersion>)> cb)
{
    // NeoForge maven metadata XML
    QString url = NEOFORGE_MAVEN + "/net/neoforged/neoforge/maven-metadata.xml";
    DownloadManager::instance().getBytes(url,
        [mcVersion, cb](bool ok, const QByteArray& data, const QString&){
            if (!ok) { cb(false, {}); return; }
            // Parse XML version list
            QList<ModLoaderVersion> list;
            QString xml = QString::fromUtf8(data);
            QRegularExpression re("<version>([^<]+)</version>");
            auto it = re.globalMatch(xml);
            while (it.hasNext()) {
                auto m = it.next();
                QString ver = m.captured(1);
                if (ver.startsWith(mcVersion.mid(2))) { // "21.1.x" for 1.21.1
                    ModLoaderVersion mv;
                    mv.mcVersion     = mcVersion;
                    mv.loaderVersion = ver;
                    mv.id            = ver;
                    mv.stable        = !ver.contains("beta") && !ver.contains("alpha");
                    list << mv;
                }
            }
            // Reverse for latest first
            std::reverse(list.begin(), list.end());
            cb(true, list);
        });
}

void ModLoaderInstaller::installFabricLike(InstanceConfig& inst,
    const QString& loaderVersion, const QString& metaBase, ModLoaderType type,
    std::function<void(int)> progressCb,
    std::function<void(bool, const QString&)> doneCb)
{
    QString url = metaBase + "/versions/loader/" + inst.mcVersion + "/" + loaderVersion + "/profile/json";
    DownloadManager::instance().getJson(url,
        [&inst, type, loaderVersion, progressCb, doneCb](bool ok, const QJsonDocument& doc, const QString& err){
            if (!ok) { doneCb(false, err); return; }
            auto profileJson = doc.object();

            // Save the loader profile JSON
            QString libDir = inst.instanceDir() + "/fabric-libs";
            QDir().mkpath(libDir);

            // Collect libraries to download
            auto libs = profileJson["libraries"].toArray();
            struct Ctx { QAtomicInt done; int total; };
            auto ctx = std::make_shared<Ctx>();
            ctx->total = 0;
            QList<DownloadTask> tasks;

            for (auto lv : libs) {
                auto lo = lv.toObject();
                QString name = lo["name"].toString();
                QString dlUrl= lo["url"].toString();
                // Convert maven name to path
                // "net.fabricmc:fabric-loader:0.14.21" -> net/fabricmc/fabric-loader/0.14.21/fabric-loader-0.14.21.jar
                QStringList parts = name.split(':');
                if (parts.size() < 3) continue;
                QString gPath = parts[0].replace('.', '/');
                QString aId   = parts[1];
                QString ver   = parts[2];
                QString jarName = aId + "-" + ver + ".jar";
                QString path  = gPath + "/" + aId + "/" + ver + "/" + jarName;
                QString dest  = AppConfig::instance().librariesDir() + "/" + path;

                if (QFile::exists(dest)) continue;
                if (dlUrl.isEmpty()) dlUrl = "https://maven.fabricmc.net/";
                QString fullUrl = dlUrl + (dlUrl.endsWith('/') ? "" : "/") + path;

                DownloadTask t;
                t.url      = fullUrl;
                t.destPath = dest;
                t.id       = "fabric-lib-" + aId + "-" + ver;
                tasks << t;
                ctx->total++;
            }

            // Update instance with loader info
            inst.modLoader        = type;
            inst.modLoaderVersion = loaderVersion;
            InstanceManager::instance().updateInstance(inst);

            if (tasks.isEmpty()) { doneCb(true, {}); return; }
            int total = ctx->total;

            for (auto& t : tasks) {
                connect(&DownloadManager::instance(), &DownloadManager::taskFinished,
                    &InstanceManager::instance(),
                    [ctx, total, progressCb, doneCb](const QString& id, bool ok2, const QString& err2){
                        if (!id.startsWith("fabric-lib-")) return;
                        int d = ctx->done.fetchAndAddRelaxed(1) + 1;
                        progressCb(d * 100 / total);
                        if (d >= total) doneCb(ok2, err2);
                    }, Qt::UniqueConnection);
                DownloadManager::instance().enqueue(t);
            }
        });
}

void ModLoaderInstaller::installFabric(InstanceConfig& inst, const QString& loaderVersion,
    std::function<void(int)> progressCb, std::function<void(bool, const QString&)> doneCb)
{
    installFabricLike(inst, loaderVersion, FABRIC_META, ModLoaderType::Fabric, progressCb, doneCb);
}

void ModLoaderInstaller::installQuilt(InstanceConfig& inst, const QString& loaderVersion,
    std::function<void(int)> progressCb, std::function<void(bool, const QString&)> doneCb)
{
    installFabricLike(inst, loaderVersion, QUILT_META, ModLoaderType::Quilt, progressCb, doneCb);
}

void ModLoaderInstaller::installForge(InstanceConfig& inst, const QString& forgeVersion,
    std::function<void(int)> progressCb, std::function<void(bool, const QString&)> doneCb)
{
    // Download Forge installer JAR
    QString mcVer   = inst.mcVersion;
    QString installer = QString("https://maven.minecraftforge.net/net/minecraftforge/forge/%1-%2/forge-%1-%2-installer.jar")
                        .arg(mcVer, forgeVersion);

    QString destJar = AppConfig::instance().tempDir() + "/forge-" + forgeVersion + "-installer.jar";
    progressCb(5);

    DownloadManager::instance().downloadFile(installer, destJar,
        [&inst, forgeVersion, destJar, progressCb, doneCb](bool ok, const QString& err){
            if (!ok) { doneCb(false, "Forge installer download failed: " + err); return; }
            progressCb(50);

            // Run installer in headless mode
            QString java = "/usr/bin/java";
            QStringList args = {"-jar", destJar, "--installClient", inst.instanceDir()};
            QProcess* proc = new QProcess;
            proc->start(java, args);
            proc->waitForFinished(300000); // 5 min timeout

            QFile::remove(destJar);
            bool success = proc->exitCode() == 0;
            proc->deleteLater();

            if (!success) { doneCb(false, "Forge installation failed"); return; }

            inst.modLoader        = ModLoaderType::Forge;
            inst.modLoaderVersion = forgeVersion;
            InstanceManager::instance().updateInstance(inst);
            progressCb(100);
            doneCb(true, {});
        });
}

void ModLoaderInstaller::installNeoForge(InstanceConfig& inst, const QString& neoForgeVersion,
    std::function<void(int)> progressCb, std::function<void(bool, const QString&)> doneCb)
{
    QString installer = QString("%1/net/neoforged/neoforge/%2/neoforge-%2-installer.jar")
                        .arg(NEOFORGE_MAVEN, neoForgeVersion);
    QString destJar   = AppConfig::instance().tempDir() + "/neoforge-" + neoForgeVersion + "-installer.jar";
    progressCb(5);

    DownloadManager::instance().downloadFile(installer, destJar,
        [&inst, neoForgeVersion, destJar, progressCb, doneCb](bool ok, const QString& err){
            if (!ok) { doneCb(false, "NeoForge installer download failed: " + err); return; }
            progressCb(50);

            QString java = "/usr/bin/java";
            QProcess* proc = new QProcess;
            proc->start(java, {"-jar", destJar, "--installClient", inst.instanceDir()});
            proc->waitForFinished(300000);
            QFile::remove(destJar);

            bool success = proc->exitCode() == 0;
            proc->deleteLater();
            if (!success) { doneCb(false, "NeoForge installation failed"); return; }

            inst.modLoader        = ModLoaderType::NeoForge;
            inst.modLoaderVersion = neoForgeVersion;
            InstanceManager::instance().updateInstance(inst);
            progressCb(100);
            doneCb(true, {});
        });
}
