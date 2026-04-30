#pragma once
#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QVariant>

class AppConfig {
public:
    static AppConfig& instance() {
        static AppConfig inst;
        return inst;
    }

    void ensureDirectories() {
        QDir().mkpath(dataDir());
        QDir().mkpath(instancesDir());
        QDir().mkpath(javaDir());
        QDir().mkpath(assetsDir());
        QDir().mkpath(librariesDir());
        QDir().mkpath(versionsDir());
        QDir().mkpath(modsDir());
        QDir().mkpath(tempDir());
    }

    QString dataDir() const {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QString instancesDir() const { return dataDir() + "/instances"; }
    QString javaDir()      const { return dataDir() + "/java"; }
    QString assetsDir()    const { return dataDir() + "/assets"; }
    QString librariesDir() const { return dataDir() + "/libraries"; }
    QString versionsDir()  const { return dataDir() + "/versions"; }
    QString modsDir()      const { return dataDir() + "/mods-cache"; }
    QString tempDir()      const { return dataDir() + "/temp"; }

    // Settings helpers
    void set(const QString& key, const QVariant& val) {
        QSettings s;
        s.setValue(key, val);
    }
    QVariant get(const QString& key, const QVariant& def = {}) const {
        QSettings s;
        return s.value(key, def);
    }
    QString getString(const QString& key, const QString& def = {}) const {
        return get(key, def).toString();
    }
    int getInt(const QString& key, int def = 0) const {
        return get(key, def).toInt();
    }
    bool getBool(const QString& key, bool def = false) const {
        return get(key, def).toBool();
    }

private:
    AppConfig() = default;
};
