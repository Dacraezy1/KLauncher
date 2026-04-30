#include "InstanceManager.h"
#include "../utils/AppConfig.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QDebug>
#include <QProcess>

QString InstanceConfig::instanceDir() const {
    return AppConfig::instance().instancesDir() + "/" + id;
}
QString InstanceConfig::modsDir()     const { return instanceDir() + "/mods"; }
QString InstanceConfig::savesDir()    const { return instanceDir() + "/saves"; }
QString InstanceConfig::configDir()   const { return instanceDir() + "/config"; }
QString InstanceConfig::screenshotsDir() const { return instanceDir() + "/screenshots"; }

QJsonObject InstanceConfig::toJson() const {
    return QJsonObject {
        {"id",               id},
        {"name",             name},
        {"iconPath",         iconPath},
        {"mcVersion",        mcVersion},
        {"modLoader",        (int)modLoader},
        {"modLoaderVersion", modLoaderVersion},
        {"javaPath",         javaPath},
        {"minRam",           minRam},
        {"maxRam",           maxRam},
        {"jvmArgs",          jvmArgs},
        {"gameDir",          gameDir},
        {"notes",            notes},
        {"createdAt",        createdAt.toString(Qt::ISODate)},
        {"lastPlayed",       lastPlayed.toString(Qt::ISODate)},
        {"playTime",         (qint64)playTime},
        {"group",            group},
        {"resolution",       resolution},
        {"fullscreen",       fullscreen},
    };
}

InstanceConfig InstanceConfig::fromJson(const QJsonObject& o) {
    InstanceConfig c;
    c.id                = o["id"].toString();
    c.name              = o["name"].toString();
    c.iconPath          = o["iconPath"].toString();
    c.mcVersion         = o["mcVersion"].toString();
    c.modLoader         = (ModLoaderType)o["modLoader"].toInt();
    c.modLoaderVersion  = o["modLoaderVersion"].toString();
    c.javaPath          = o["javaPath"].toString();
    c.minRam            = o["minRam"].toInt(512);
    c.maxRam            = o["maxRam"].toInt(2048);
    c.jvmArgs           = o["jvmArgs"].toString();
    c.gameDir           = o["gameDir"].toString();
    c.notes             = o["notes"].toString();
    c.createdAt         = QDateTime::fromString(o["createdAt"].toString(), Qt::ISODate);
    c.lastPlayed        = QDateTime::fromString(o["lastPlayed"].toString(), Qt::ISODate);
    c.playTime          = (qint64)o["playTime"].toDouble();
    c.group             = o["group"].toString();
    c.resolution        = o["resolution"].toString();
    c.fullscreen        = o["fullscreen"].toBool();
    return c;
}

InstanceManager::InstanceManager(QObject* parent)
    : QObject(parent)
    , m_configPath(AppConfig::instance().dataDir() + "/instances.json")
{
    load();
}

InstanceManager& InstanceManager::instance() {
    static InstanceManager inst;
    return inst;
}

InstanceConfig InstanceManager::findById(const QString& id) const {
    for (auto& i : m_instances) if (i.id == id) return i;
    return {};
}

void InstanceManager::createInstance(const InstanceConfig& cfg) {
    InstanceConfig c = cfg;
    if (c.id.isEmpty())
        c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!c.createdAt.isValid())
        c.createdAt = QDateTime::currentDateTime();

    // Create dirs
    QDir().mkpath(c.instanceDir());
    QDir().mkpath(c.modsDir());
    QDir().mkpath(c.savesDir());
    QDir().mkpath(c.configDir());

    m_instances << c;
    save();
    emit instanceCreated(c);
    emit instancesChanged();
}

void InstanceManager::updateInstance(const InstanceConfig& cfg) {
    for (auto& i : m_instances) {
        if (i.id == cfg.id) {
            i = cfg;
            save();
            emit instancesChanged();
            return;
        }
    }
}

void InstanceManager::deleteInstance(const QString& id) {
    auto it = std::remove_if(m_instances.begin(), m_instances.end(),
        [&id](const InstanceConfig& c){ return c.id == id; });
    if (it != m_instances.end()) {
        QString dir = it->instanceDir();
        m_instances.erase(it, m_instances.end());
        // Remove directory
        QDir(dir).removeRecursively();
        save();
        emit instanceDeleted(id);
        emit instancesChanged();
    }
}

void InstanceManager::duplicateInstance(const QString& id) {
    auto src = findById(id);
    if (src.id.isEmpty()) return;
    InstanceConfig copy = src;
    copy.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    copy.name      = src.name + " (Copy)";
    copy.createdAt = QDateTime::currentDateTime();
    copy.lastPlayed = {};
    copy.playTime  = 0;
    createInstance(copy);

    // Copy files
    QDir srcDir(src.instanceDir());
    QDir dstDir(copy.instanceDir());
    auto copyRecursive = [](const QString& src, const QString& dst, auto& self) -> void {
        QDir(dst).mkpath(".");
        for (auto& entry : QDir(src).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.isDir()) self(entry.filePath(), dst + "/" + entry.fileName(), self);
            else QFile::copy(entry.filePath(), dst + "/" + entry.fileName());
        }
    };
    copyRecursive(src.instanceDir(), copy.instanceDir(), copyRecursive);
}

void InstanceManager::load() {
    QFile f(m_configPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto arr = QJsonDocument::fromJson(f.readAll()).array();
    m_instances.clear();
    for (auto v : arr)
        m_instances << InstanceConfig::fromJson(v.toObject());
}

void InstanceManager::save() {
    QJsonArray arr;
    for (auto& i : m_instances) arr << i.toJson();
    QFile f(m_configPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson());
}
