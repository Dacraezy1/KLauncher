#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include "InstanceManager.h"
#include "../auth/MicrosoftAuth.h"

class GameLauncher : public QObject {
    Q_OBJECT
public:
    explicit GameLauncher(QObject* parent = nullptr);
    static GameLauncher& instance();

    void launch(const InstanceConfig& instance,
                const MicrosoftAccount& account,   // empty = offline
                const QString& offlineUsername = {});

    void kill(const QString& instanceId);
    bool isRunning(const QString& instanceId) const;

signals:
    void gameStarted(const QString& instanceId);
    void gameStopped(const QString& instanceId, int exitCode);
    void gameLog(const QString& instanceId, const QString& line);
    void gameError(const QString& instanceId, const QString& msg);

private:
    struct RunningGame {
        QProcess* process;
        QString   instanceId;
    };
    QList<RunningGame> m_running;

    QString buildClasspath(const InstanceConfig& inst,
                           const QJsonObject& versionJson) const;
    QString resolveJava(const InstanceConfig& inst,
                        const QJsonObject& versionJson) const;
    QStringList buildJvmArgs(const InstanceConfig& inst,
                             const QJsonObject& versionJson,
                             const QString& nativesDir) const;
    QString buildMainClass(const QJsonObject& versionJson,
                           const InstanceConfig& inst) const;
    QStringList buildGameArgs(const InstanceConfig& inst,
                              const QJsonObject& versionJson,
                              const MicrosoftAccount& acc,
                              const QString& offlineUsername) const;
    void extractNatives(const QJsonObject& versionJson,
                        const QString& nativesDir) const;
};
