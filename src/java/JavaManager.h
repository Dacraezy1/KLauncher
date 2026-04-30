#pragma once
#include <QObject>
#include <QString>
#include <QList>

struct JavaInstall {
    QString path;       // path to java executable
    int     majorVersion = 0;
    QString fullVersion;
    QString vendor;
    bool    bundled = false;  // came with KLauncher
};

class JavaManager : public QObject {
    Q_OBJECT
public:
    explicit JavaManager(QObject* parent = nullptr);
    static JavaManager& instance();

    const QList<JavaInstall>& installs() const { return m_installs; }

    // Scan system for Java installations
    void detectJava();

    // Find best Java for given major version (exact or higher)
    QString findBestJava(int majorVersion) const;

    // Download Adoptium/Temurin JRE
    void downloadJava(int majorVersion,
                      std::function<void(int)> progressCb,
                      std::function<void(bool, const QString&)> doneCb);

    static int detectJavaMajor(const QString& javaPath);
    static QString detectJavaVersion(const QString& javaPath);

signals:
    void detectFinished();
    void downloadProgress(int percent);

private:
    QList<JavaInstall> m_installs;
    void scanPath(const QString& dir);

    // Adoptium API
    static const QString ADOPTIUM_API;
};
