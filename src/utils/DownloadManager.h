#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QFile>
#include <QQueue>
#include <QHash>
#include <functional>

struct DownloadTask {
    QString url;
    QString destPath;
    QString sha1;        // optional checksum
    QString id;          // unique task id
    qint64 totalSize = 0;
    qint64 received  = 0;
};

class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(QObject* parent = nullptr);
    ~DownloadManager() override = default;

    static DownloadManager& instance();

    // Enqueue a single file download
    void enqueue(const DownloadTask& task);

    // Download a file immediately (blocking via event loop helper)
    void downloadFile(const QString& url, const QString& dest,
                      std::function<void(bool, const QString&)> callback);

    // Simple GET returning bytes
    void getBytes(const QString& url,
                  std::function<void(bool, const QByteArray&, const QString&)> callback);

    void getJson(const QString& url,
                 std::function<void(bool, const QJsonDocument&, const QString&)> callback);

    void cancelAll();

    int pendingCount() const { return m_queue.size() + m_active.size(); }

signals:
    void taskStarted(const QString& id);
    void taskProgress(const QString& id, qint64 received, qint64 total);
    void taskFinished(const QString& id, bool success, const QString& error);
    void allFinished();

private slots:
    void processQueue();
    void onReadyRead();
    void onProgress(qint64 received, qint64 total);
    void onFinished();

private:
    QNetworkAccessManager* m_nam;
    QQueue<DownloadTask>   m_queue;
    QHash<QNetworkReply*, DownloadTask> m_active;
    QHash<QNetworkReply*, QFile*>       m_files;
    static const int MAX_CONCURRENT = 4;

    void startNext();
    bool verifySha1(const QString& path, const QString& expected);
};
