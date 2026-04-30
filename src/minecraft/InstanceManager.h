#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QDateTime>

enum class ModLoaderType {
    None,
    Forge,
    Fabric,
    Quilt,
    NeoForge
};

struct InstanceConfig {
    QString  id;
    QString  name;
    QString  iconPath;
    QString  mcVersion;
    ModLoaderType modLoader = ModLoaderType::None;
    QString  modLoaderVersion;
    QString  javaPath;       // empty = auto-detect
    int      minRam = 512;
    int      maxRam = 2048;
    QString  jvmArgs;
    QString  gameDir;        // empty = use instance dir
    QString  notes;
    QDateTime createdAt;
    QDateTime lastPlayed;
    qint64    playTime = 0;  // seconds
    QString  group;
    QString  resolution;     // e.g. "1280x720"
    bool     fullscreen = false;

    QString instanceDir() const;
    QString modsDir()     const;
    QString savesDir()    const;
    QString configDir()   const;
    QString screenshotsDir() const;

    QJsonObject toJson() const;
    static InstanceConfig fromJson(const QJsonObject& obj);
};

class InstanceManager : public QObject {
    Q_OBJECT
public:
    explicit InstanceManager(QObject* parent = nullptr);
    static InstanceManager& instance();

    QList<InstanceConfig> instances() const { return m_instances; }
    InstanceConfig findById(const QString& id) const;

    void createInstance(const InstanceConfig& cfg);
    void updateInstance(const InstanceConfig& cfg);
    void deleteInstance(const QString& id);
    void duplicateInstance(const QString& id);

    void importInstance(const QString& zipPath);
    void exportInstance(const QString& id, const QString& zipPath);

    void load();
    void save();

signals:
    void instancesChanged();
    void instanceCreated(const InstanceConfig& cfg);
    void instanceDeleted(const QString& id);

private:
    QList<InstanceConfig> m_instances;
    QString               m_configPath;
};
