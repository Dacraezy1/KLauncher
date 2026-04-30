#include "GameLauncher.h"
#include "VersionFetcher.h"
#include "../utils/AppConfig.h"
#include "../java/JavaManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QDebug>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>
#include <quazipfile.h>  // from quazip, we'll handle this via extraction fallback

GameLauncher::GameLauncher(QObject* parent) : QObject(parent) {}

GameLauncher& GameLauncher::instance() {
    static GameLauncher inst;
    return inst;
}

void GameLauncher::launch(const InstanceConfig& inst,
                          const MicrosoftAccount& account,
                          const QString& offlineUsername)
{
    if (isRunning(inst.id)) {
        emit gameError(inst.id, "Instance already running");
        return;
    }

    auto vJson = VersionFetcher::instance().loadVersionJson(inst.mcVersion);
    if (vJson.isEmpty()) {
        emit gameError(inst.id, "Version JSON not found for " + inst.mcVersion);
        return;
    }

    QString nativesDir = AppConfig::instance().tempDir()
                        + "/natives-" + inst.id + "-"
                        + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(nativesDir);
    extractNatives(vJson, nativesDir);

    QString java = resolveJava(inst, vJson);
    if (java.isEmpty()) {
        emit gameError(inst.id, "No suitable Java found. Please install Java.");
        return;
    }

    QStringList args;
    // JVM args
    args << buildJvmArgs(inst, vJson, nativesDir);
    // Main class
    args << buildMainClass(vJson, inst);
    // Game args
    args << buildGameArgs(inst, vJson, account, offlineUsername);

    auto* proc = new QProcess(this);
    QString workDir = inst.gameDir.isEmpty() ? inst.instanceDir() : inst.gameDir;
    QDir().mkpath(workDir);
    proc->setWorkingDirectory(workDir);

    RunningGame rg;
    rg.process    = proc;
    rg.instanceId = inst.id;
    m_running << rg;

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, id = inst.id](){
        while (proc->canReadLine())
            emit gameLog(id, QString::fromUtf8(proc->readLine()).trimmed());
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc, id = inst.id](){
        while (proc->canReadLine())
            emit gameLog(id, "[STDERR] " + QString::fromUtf8(proc->readLine()).trimmed());
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, id = inst.id, nativesDir](int code, QProcess::ExitStatus) {
            m_running.erase(std::remove_if(m_running.begin(), m_running.end(),
                [&id](const RunningGame& rg){ return rg.instanceId == id; }), m_running.end());
            proc->deleteLater();
            QDir(nativesDir).removeRecursively();
            emit gameStopped(id, code);
        });

    qDebug() << "Launching:" << java << args;
    proc->start(java, args);

    if (!proc->waitForStarted(5000)) {
        emit gameError(inst.id, "Failed to start Java process");
        proc->deleteLater();
        m_running.removeLast();
        return;
    }
    emit gameStarted(inst.id);
}

void GameLauncher::kill(const QString& instanceId) {
    for (auto& rg : m_running) {
        if (rg.instanceId == instanceId) {
            rg.process->kill();
            return;
        }
    }
}

bool GameLauncher::isRunning(const QString& instanceId) const {
    for (auto& rg : m_running)
        if (rg.instanceId == instanceId) return true;
    return false;
}

QString GameLauncher::resolveJava(const InstanceConfig& inst,
                                   const QJsonObject& vJson) const
{
    if (!inst.javaPath.isEmpty() && QFile::exists(inst.javaPath))
        return inst.javaPath;
    // Determine recommended Java version from version JSON
    int majorNeeded = 8;
    auto compInfo = vJson["javaVersion"].toObject();
    if (!compInfo.isEmpty())
        majorNeeded = compInfo["majorVersion"].toInt(8);

    return JavaManager::instance().findBestJava(majorNeeded);
}

QString GameLauncher::buildClasspath(const InstanceConfig& inst,
                                      const QJsonObject& vJson) const
{
    QStringList cp;
    auto libs = vJson["libraries"].toArray();
    for (auto lv : libs) {
        auto lo = lv.toObject();
        bool allowed = true;
        if (lo.contains("rules")) {
            allowed = false;
            for (auto rv : lo["rules"].toArray()) {
                auto ro = rv.toObject();
                if (!ro.contains("os") && ro["action"].toString() == "allow") { allowed = true; }
                else if (ro["os"].toObject()["name"].toString() == "linux"
                         && ro["action"].toString() == "allow") { allowed = true; }
            }
        }
        if (!allowed) continue;
        auto artifact = lo["downloads"].toObject()["artifact"].toObject();
        QString path = artifact["path"].toString();
        if (path.isEmpty()) continue;
        QString full = AppConfig::instance().librariesDir() + "/" + path;
        if (QFile::exists(full)) cp << full;
    }

    // Add modloader libs
    if (inst.modLoader == ModLoaderType::Fabric || inst.modLoader == ModLoaderType::Quilt) {
        QString fabricDir = inst.instanceDir() + "/fabric-libs";
        if (QDir(fabricDir).exists()) {
            for (auto& fi : QDir(fabricDir).entryInfoList({"*.jar"}, QDir::Files))
                cp << fi.absoluteFilePath();
        }
    }
    if (inst.modLoader == ModLoaderType::Forge || inst.modLoader == ModLoaderType::NeoForge) {
        QString forgeDir = inst.instanceDir() + "/forge-libs";
        if (QDir(forgeDir).exists()) {
            for (auto& fi : QDir(forgeDir).entryInfoList({"*.jar"}, QDir::Files))
                cp << fi.absoluteFilePath();
        }
    }

    // Version jar itself
    cp << VersionFetcher::instance().versionJarPath(inst.mcVersion);

    return cp.join(":");
}

QStringList GameLauncher::buildJvmArgs(const InstanceConfig& inst,
                                        const QJsonObject& vJson,
                                        const QString& nativesDir) const
{
    QStringList args;
    args << QString("-Xms%1m").arg(inst.minRam);
    args << QString("-Xmx%1m").arg(inst.maxRam);

    // Performance JVM flags
    args << "-XX:+UnlockExperimentalVMOptions"
         << "-XX:+UseG1GC"
         << "-XX:G1NewSizePercent=20"
         << "-XX:G1ReservePercent=20"
         << "-XX:MaxGCPauseMillis=50"
         << "-XX:G1HeapRegionSize=32M"
         << "-XX:+DisableExplicitGC"
         << "-XX:+AlwaysPreTouch"
         << "-XX:+ParallelRefProcEnabled"
         << "-Dfile.encoding=UTF-8"
         << "-Dstdout.encoding=UTF-8";

    args << "-Djava.library.path=" + nativesDir;
    args << "-cp" << buildClasspath(inst, vJson);

    // Extra user JVM args
    if (!inst.jvmArgs.isEmpty()) {
        for (auto& a : inst.jvmArgs.split(' ', Qt::SkipEmptyParts))
            args << a;
    }
    return args;
}

QString GameLauncher::buildMainClass(const QJsonObject& vJson,
                                      const InstanceConfig& inst) const
{
    if (inst.modLoader == ModLoaderType::Fabric || inst.modLoader == ModLoaderType::Quilt)
        return "net.fabricmc.loader.impl.launch.knot.KnotClient";
    if (inst.modLoader == ModLoaderType::Forge || inst.modLoader == ModLoaderType::NeoForge) {
        // Forge overrides main class in its version JSON, parsed separately
    }
    return vJson["mainClass"].toString("net.minecraft.client.main.Main");
}

QStringList GameLauncher::buildGameArgs(const InstanceConfig& inst,
                                         const QJsonObject& vJson,
                                         const MicrosoftAccount& acc,
                                         const QString& offlineUsername) const
{
    bool online       = acc.valid && !acc.accessToken.isEmpty();
    QString username  = online ? acc.username : (offlineUsername.isEmpty() ? "Player" : offlineUsername);
    QString uuid      = online ? acc.uuid     : "00000000-0000-0000-0000-000000000000";
    QString token     = online ? acc.accessToken : "0";
    QString userType  = online ? "msa" : "legacy";
    QString gameDir   = inst.gameDir.isEmpty() ? inst.instanceDir() : inst.gameDir;
    QString assetsDir = AppConfig::instance().assetsDir();
    QString assetIdx  = vJson["assetIndex"].toObject()["id"].toString("legacy");
    QString version   = inst.mcVersion;

    QStringList args;

    // Modern argument format (1.13+)
    auto argsObj = vJson["arguments"].toObject();
    if (!argsObj.isEmpty()) {
        auto gameArgsArr = argsObj["game"].toArray();
        for (auto v : gameArgsArr) {
            if (v.isString()) {
                QString a = v.toString();
                a.replace("${auth_player_name}", username)
                 .replace("${version_name}", version)
                 .replace("${game_directory}", gameDir)
                 .replace("${assets_root}", assetsDir)
                 .replace("${assets_index_name}", assetIdx)
                 .replace("${auth_uuid}", uuid)
                 .replace("${auth_access_token}", token)
                 .replace("${user_type}", userType)
                 .replace("${version_type}", "KLauncher");
                args << a;
            }
            // skip rule-based args for simplicity
        }
    } else {
        // Legacy format
        QString legacy = vJson["minecraftArguments"].toString();
        legacy.replace("${auth_player_name}", username)
              .replace("${version_name}", version)
              .replace("${game_directory}", gameDir)
              .replace("${assets_root}", assetsDir)
              .replace("${assets_index_name}", assetIdx)
              .replace("${auth_uuid}", uuid)
              .replace("${auth_access_token}", token)
              .replace("${user_type}", userType)
              .replace("${version_type}", "KLauncher");
        args << legacy.split(' ', Qt::SkipEmptyParts);
    }

    // Resolution
    if (!inst.resolution.isEmpty()) {
        auto parts = inst.resolution.split('x');
        if (parts.size() == 2) {
            args << "--width"  << parts[0];
            args << "--height" << parts[1];
        }
    }
    if (inst.fullscreen) args << "--fullscreen";

    return args;
}

void GameLauncher::extractNatives(const QJsonObject& vJson,
                                   const QString& nativesDir) const
{
    auto libs = vJson["libraries"].toArray();
    for (auto lv : libs) {
        auto lo = lv.toObject();
        auto classifiers = lo["downloads"].toObject()["classifiers"].toObject();
        QString key = "natives-linux";
        if (!classifiers.contains(key)) continue;

        auto nativeArt  = classifiers[key].toObject();
        QString path    = nativeArt["path"].toString();
        QString jarPath = AppConfig::instance().librariesDir() + "/" + path;

        if (!QFile::exists(jarPath)) continue;

        // Extract jar (zip) into nativesDir
        QProcess unzip;
        unzip.start("unzip", {"-o", "-q", jarPath, "-d", nativesDir});
        unzip.waitForFinished(10000);
    }
}
