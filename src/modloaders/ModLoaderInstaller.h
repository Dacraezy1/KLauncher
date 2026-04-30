#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "../minecraft/InstanceManager.h"

struct ModLoaderVersion {
    QString id;
    QString mcVersion;
    QString loaderVersion;
    bool    stable = true;
};

class ModLoaderInstaller : public QObject {
    Q_OBJECT
public:
    explicit ModLoaderInstaller(QObject* parent = nullptr);
    static ModLoaderInstaller& instance();

    // Fetch available loader versions for given MC version
    void fetchFabricVersions(const QString& mcVersion,
        std::function<void(bool, QList<ModLoaderVersion>)> cb);

    void fetchQuiltVersions(const QString& mcVersion,
        std::function<void(bool, QList<ModLoaderVersion>)> cb);

    void fetchForgeVersions(const QString& mcVersion,
        std::function<void(bool, QList<ModLoaderVersion>)> cb);

    void fetchNeoForgeVersions(const QString& mcVersion,
        std::function<void(bool, QList<ModLoaderVersion>)> cb);

    // Install a loader into an instance
    void installFabric(InstanceConfig& inst, const QString& loaderVersion,
        std::function<void(int)> progressCb,
        std::function<void(bool, const QString&)> doneCb);

    void installForge(InstanceConfig& inst, const QString& forgeVersion,
        std::function<void(int)> progressCb,
        std::function<void(bool, const QString&)> doneCb);

    void installNeoForge(InstanceConfig& inst, const QString& neoForgeVersion,
        std::function<void(int)> progressCb,
        std::function<void(bool, const QString&)> doneCb);

    void installQuilt(InstanceConfig& inst, const QString& loaderVersion,
        std::function<void(int)> progressCb,
        std::function<void(bool, const QString&)> doneCb);

private:
    static const QString FABRIC_META;
    static const QString QUILT_META;
    static const QString FORGE_MAVEN;
    static const QString NEOFORGE_MAVEN;

    void installFabricLike(InstanceConfig& inst, const QString& loaderVersion,
        const QString& metaBase, ModLoaderType type,
        std::function<void(int)> progressCb,
        std::function<void(bool, const QString&)> doneCb);
};
