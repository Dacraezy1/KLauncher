#include "DownloadManager.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QTimer>
#include <QUuid>
#include <QDebug>

DownloadManager::DownloadManager(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

DownloadManager& DownloadManager::instance() {
    static DownloadManager inst;
    return inst;
}

void DownloadManager::enqueue(const DownloadTask& task) {
    m_queue.enqueue(task);
    QTimer::singleShot(0, this, &DownloadManager::processQueue);
}

void DownloadManager::downloadFile(const QString& url, const QString& dest,
    std::function<void(bool, const QString&)> callback)
{
    DownloadTask t;
    t.url      = url;
    t.destPath = dest;
    t.id       = QUuid::createUuid().toString(QUuid::WithoutBraces);

    connect(this, &DownloadManager::taskFinished, this,
        [this, t, callback](const QString& id, bool ok, const QString& err) {
            if (id == t.id) {
                callback(ok, err);
                // disconnect handled by QObject cleanup
            }
        }, Qt::SingleShotConnection);

    enqueue(t);
}

void DownloadManager::getBytes(const QString& url,
    std::function<void(bool, const QByteArray&, const QString&)> callback)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "KLauncher/1.0");
    auto* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        if (reply->error() == QNetworkReply::NoError) {
            callback(true, reply->readAll(), {});
        } else {
            callback(false, {}, reply->errorString());
        }
        reply->deleteLater();
    });
}

void DownloadManager::getJson(const QString& url,
    std::function<void(bool, const QJsonDocument&, const QString&)> callback)
{
    getBytes(url, [callback](bool ok, const QByteArray& data, const QString& err) {
        if (!ok) { callback(false, {}, err); return; }
        QJsonParseError pe;
        auto doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) {
            callback(false, {}, pe.errorString());
        } else {
            callback(true, doc, {});
        }
    });
}

void DownloadManager::cancelAll() {
    for (auto* reply : m_active.keys()) reply->abort();
    m_queue.clear();
}

void DownloadManager::processQueue() {
    while (m_active.size() < MAX_CONCURRENT && !m_queue.isEmpty()) {
        startNext();
    }
}

void DownloadManager::startNext() {
    if (m_queue.isEmpty()) return;
    DownloadTask task = m_queue.dequeue();

    // Ensure dir
    QFileInfo fi(task.destPath);
    QDir().mkpath(fi.absolutePath());

    auto* file = new QFile(task.destPath, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit taskFinished(task.id, false, "Cannot open: " + task.destPath);
        file->deleteLater();
        processQueue();
        return;
    }

    QNetworkRequest req(task.url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "KLauncher/1.0");
    auto* reply = m_nam->get(req);

    m_active.insert(reply, task);
    m_files.insert(reply, file);

    connect(reply, &QNetworkReply::readyRead, this, &DownloadManager::onReadyRead);
    connect(reply, &QNetworkReply::downloadProgress, this, &DownloadManager::onProgress);
    connect(reply, &QNetworkReply::finished, this, &DownloadManager::onFinished);

    emit taskStarted(task.id);
}

void DownloadManager::onReadyRead() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    auto* file = m_files.value(reply);
    if (file) file->write(reply->readAll());
}

void DownloadManager::onProgress(qint64 received, qint64 total) {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    const auto& task = m_active[reply];
    emit taskProgress(task.id, received, total);
}

void DownloadManager::onFinished() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    DownloadTask task = m_active.take(reply);
    QFile* file = m_files.take(reply);

    if (file) {
        file->write(reply->readAll());
        file->flush();
        file->close();
        file->deleteLater();
    }

    bool success = (reply->error() == QNetworkReply::NoError);
    QString err;
    if (!success) {
        err = reply->errorString();
        QFile::remove(task.destPath);
    } else if (!task.sha1.isEmpty()) {
        if (!verifySha1(task.destPath, task.sha1)) {
            success = false;
            err = "SHA1 mismatch for " + task.destPath;
            QFile::remove(task.destPath);
        }
    }

    reply->deleteLater();
    emit taskFinished(task.id, success, err);

    if (m_active.isEmpty() && m_queue.isEmpty()) {
        emit allFinished();
    } else {
        processQueue();
    }
}

bool DownloadManager::verifySha1(const QString& path, const QString& expected) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&f);
    return hash.result().toHex() == expected.toLower().toUtf8();
}
