#ifndef DOWNLOAD_H
#define DOWNLOAD_H
#include <QObject>
#include <QJsonDocument>
#include <QtNetwork/QNetworkReply>
enum TYPES {YDISK, JSON, BINFILE};

class DownloadActions: public QObject
{
    Q_OBJECT
public:
    virtual ~DownloadActions() = default;
signals:
    void jsonFineshed(const QJsonDocument &jdoc);
    void fileFineshed(const QString &filename);
    void progress(int progress);
    void cancel();
};

typedef QSharedPointer<DownloadActions> DownloadActionsPtr;

class DownloadManager: public QObject
{
    Q_OBJECT
    QNetworkAccessManager manager;
    QVector<QNetworkReply *> currentDownloads;
    QMap<QNetworkReply *, int> type;
    QMap<QNetworkReply *, DownloadActionsPtr> actions;

public:
    DownloadManager();
    DownloadActionsPtr doDownload(const QUrl &url, int type=YDISK, DownloadActionsPtr responce=DownloadActionsPtr(new DownloadActions()));
    DownloadActionsPtr doDownloadJSON(const QUrl &url);
    static QString saveFileName(const QUrl &url);
    bool saveToDisk(const QString &filename, QIODevice *data);
    static bool isHttpRedirect(QNetworkReply *reply);
    static QString getHref(QIODevice *data);
signals:
    void jsonFineshed(QNetworkReply* reply, const QJsonDocument &jdoc);
public slots:
    void downloadFinished(QNetworkReply *reply);
    void sslErrors(const QList<QSslError> &errors);
    void emitProgress(qint64 bytesReceived, qint64 bytesTotal);
};


#endif // DOWNLOAD_H
