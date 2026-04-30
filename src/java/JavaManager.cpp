#include "JavaManager.h"
#include "../utils/AppConfig.h"
#include "../utils/DownloadManager.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

const QString JavaManager::ADOPTIUM_API =
    "https://api.adoptium.net/v3/assets/latest/%1/hotspot?architecture=x64&image_type=jre&os=linux&vendor=eclipse";

JavaManager::JavaManager(QObject* parent) : QObject(parent) {
    detectJava();
}

JavaManager& JavaManager::instance() {
    static JavaManager inst;
    return inst;
}

void JavaManager::detectJava() {
    m_installs.clear();

    // 1. Check bundled java dir
    QString bundledBase = AppConfig::instance().javaDir();
    for (auto& d : QDir(bundledBase).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString javaExe = d.absoluteFilePath() + "/bin/java";
        if (QFile::exists(javaExe)) {
            JavaInstall ji;
            ji.path        = javaExe;
            ji.bundled     = true;
            ji.majorVersion= detectJavaMajor(javaExe);
            ji.fullVersion = detectJavaVersion(javaExe);
            if (ji.majorVersion > 0) m_installs << ji;
        }
    }

    // 2. Common system paths on Linux
    QStringList systemDirs = {
        "/usr/lib/jvm",
        "/usr/local/lib/jvm",
        "/opt/java",
        "/opt/jdk",
        "/opt/jre",
    };
    for (auto& d : systemDirs) scanPath(d);

    // 3. PATH
    auto pathDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    QString pathEnv = qgetenv("PATH");
    for (auto& p : pathEnv.split(':')) {
        QString javaExe = p + "/java";
        if (QFile::exists(javaExe)) {
            JavaInstall ji;
            ji.path        = javaExe;
            ji.majorVersion= detectJavaMajor(javaExe);
            ji.fullVersion = detectJavaVersion(javaExe);
            if (ji.majorVersion > 0 && std::none_of(m_installs.begin(), m_installs.end(),
                    [&ji](const JavaInstall& x){ return x.path == ji.path; }))
                m_installs << ji;
        }
    }

    // Sort by major version descending
    std::sort(m_installs.begin(), m_installs.end(),
        [](const JavaInstall& a, const JavaInstall& b){
            return a.majorVersion > b.majorVersion;
        });

    emit detectFinished();
}

void JavaManager::scanPath(const QString& dir) {
    if (!QDir(dir).exists()) return;
    for (auto& d : QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString javaExe = d.absoluteFilePath() + "/bin/java";
        if (!QFile::exists(javaExe)) {
            // Try one level deeper
            for (auto& d2 : QDir(d.absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                javaExe = d2.absoluteFilePath() + "/bin/java";
                if (QFile::exists(javaExe)) {
                    JavaInstall ji;
                    ji.path        = javaExe;
                    ji.majorVersion= detectJavaMajor(javaExe);
                    ji.fullVersion = detectJavaVersion(javaExe);
                    if (ji.majorVersion > 0) m_installs << ji;
                }
            }
        } else {
            JavaInstall ji;
            ji.path        = javaExe;
            ji.majorVersion= detectJavaMajor(javaExe);
            ji.fullVersion = detectJavaVersion(javaExe);
            if (ji.majorVersion > 0) m_installs << ji;
        }
    }
}

QString JavaManager::findBestJava(int majorVersion) const {
    // Prefer exact match, otherwise lowest that's >= needed
    for (auto& j : m_installs)
        if (j.majorVersion == majorVersion) return j.path;
    for (auto& j : m_installs)
        if (j.majorVersion > majorVersion) return j.path;
    return {};
}

int JavaManager::detectJavaMajor(const QString& javaPath) {
    QProcess p;
    p.start(javaPath, {"-version"});
    p.waitForFinished(5000);
    QString out = p.readAllStandardError() + p.readAllStandardOutput();
    // "version \"17.0.1\"" or "version \"1.8.0_301\""
    QRegularExpression re(R"(version "(\d+)(?:\.(\d+))?)");;
    auto m = re.match(out);
    if (!m.hasMatch()) return 0;
    int major = m.captured(1).toInt();
    if (major == 1) major = m.captured(2).toInt(); // 1.8 -> 8
    return major;
}

QString JavaManager::detectJavaVersion(const QString& javaPath) {
    QProcess p;
    p.start(javaPath, {"-version"});
    p.waitForFinished(5000);
    return (p.readAllStandardError() + p.readAllStandardOutput()).trimmed().split('\n').first();
}

void JavaManager::downloadJava(int majorVersion,
    std::function<void(int)> progressCb,
    std::function<void(bool, const QString&)> doneCb)
{
    QString url = ADOPTIUM_API.arg(majorVersion);
    DownloadManager::instance().getJson(url,
        [this, majorVersion, progressCb, doneCb](bool ok, const QJsonDocument& doc, const QString& err){
            if (!ok) { doneCb(false, err); return; }
            auto arr = doc.array();
            if (arr.isEmpty()) { doneCb(false, "No Java " + QString::number(majorVersion) + " found"); return; }

            auto asset  = arr[0].toObject();
            auto binary = asset["binary"].toObject();
            auto pkg    = binary["package"].toObject();
            QString downloadUrl  = pkg["link"].toString();
            QString filename     = pkg["name"].toString();

            QString destDir  = AppConfig::instance().javaDir() + "/jre-" + QString::number(majorVersion);
            QString tarPath  = AppConfig::instance().tempDir() + "/" + filename;
            QDir().mkpath(AppConfig::instance().tempDir());

            DownloadTask t;
            t.url      = downloadUrl;
            t.destPath = tarPath;
            t.id       = "java-dl-" + QString::number(majorVersion);

            connect(&DownloadManager::instance(), &DownloadManager::taskProgress, this,
                [progressCb](const QString& id, qint64 recv, qint64 total){
                    if (!id.startsWith("java-dl-")) return;
                    if (total > 0) progressCb((int)(recv * 100 / total));
                }, Qt::UniqueConnection);

            connect(&DownloadManager::instance(), &DownloadManager::taskFinished, this,
                [this, tarPath, destDir, majorVersion, doneCb](const QString& id, bool ok2, const QString& err2){
                    if (!id.startsWith("java-dl-")) return;
                    if (!ok2) { doneCb(false, err2); return; }

                    // Extract tar.gz
                    QDir().mkpath(destDir);
                    QProcess tar;
                    tar.start("tar", {"xzf", tarPath, "-C", destDir, "--strip-components=1"});
                    tar.waitForFinished(120000);
                    QFile::remove(tarPath);

                    if (tar.exitCode() != 0) {
                        doneCb(false, "Extraction failed");
                        return;
                    }
                    detectJava();
                    doneCb(true, {});
                }, Qt::SingleShotConnection);

            DownloadManager::instance().enqueue(t);
        });
}
