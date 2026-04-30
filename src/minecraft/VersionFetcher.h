#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

struct MCVersion {
    QString id;
    QString type;        // "release", "snapshot", "old_beta", "old_alpha"
    QString url;
    QString releaseTime;
    bool    installed = false;
};

class VersionFetcher : public QObject {
    Q_OBJECT
public:
    explicit VersionFetcher(QObject* parent = nullptr);
    static VersionFetcher& instance();

    const QList<MCVersion>& versions() const { return m_versions; }
    QList<MCVersion> releases()  const;
    QList<MCVersion> snapshots() const;

    void fetchVersionList(std::function<void(bool)> cb = nullptr);
    void downloadVersion(const QString& versionId,
                         std::function<void(bool, const QString&)> cb);

    bool isInstalled(const QString& versionId) const;
    QString versionJsonPath(const QString& versionId) const;
    QString versionJarPath(const QString& versionId) const;

    QJsonObject loadVersionJson(const QString& versionId) const;
    void downloadAssets(const QJsonObject& versionJson,
                        std::function<void(int,int)> progressCb,
                        std::function<void(bool)> doneCb);
    void downloadLibraries(const QJsonObject& versionJson,
                           std::function<void(int,int)> progressCb,
                           std::function<void(bool)> doneCb);

signals:
    void versionsLoaded();
    void downloadProgress(const QString& versionId, int percent);

private:
    QList<MCVersion> m_versions;
    static const QString MANIFEST_URL;
    void checkInstalled();
};
