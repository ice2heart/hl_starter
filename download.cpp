#include "download.h"
#include <QFileInfo>
#include <QJsonObject>

DownloadManager::DownloadManager()
{
    connect(&manager, SIGNAL(finished(QNetworkReply*)),
            SLOT(downloadFinished(QNetworkReply*)));
}

DownloadActionsPtr DownloadManager::doDownloadJSON(const QUrl &url)
{
    return doDownload(url, JSON);
}

DownloadActionsPtr DownloadManager::doDownload(const QUrl &url, int type, DownloadActionsPtr responce)
{
    QUrl realUrl(url);
    if (realUrl.toString().startsWith("https://yadi.sk/")){
        realUrl = QUrl(QString("https://cloud-api.yandex.net/v1/disk/public/resources/download?public_key=%1").arg(url.toString()));
        type = YDISK;
    }
    QNetworkRequest request(realUrl);
    QNetworkReply *reply = manager.get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, &DownloadManager::emitProgress);
    connect(responce.data(), &DownloadActions::cancel, reply, &QNetworkReply::abort);
    this->type.insert(reply, type);

    actions.insert(reply, responce);
#if QT_CONFIG(ssl)
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            SLOT(sslErrors(QList<QSslError>)));
#endif

    currentDownloads.append(reply);
    return responce;
}

QString DownloadManager::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = QFileInfo(path).fileName();

    if (basename.isEmpty())
        basename = "download";

    if (QFile::exists(basename)) {
        // already exists, don't overwrite
        int i = 0;
        basename += '.';
        while (QFile::exists(basename + QString::number(i)))
            ++i;

        basename += QString::number(i);
    }

    return basename;
}

bool DownloadManager::saveToDisk(const QString &filename, QIODevice *data)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        fprintf(stderr, "Could not open %s for writing: %s\n",
                qPrintable(filename),
                qPrintable(file.errorString()));
        return false;
    }

    file.write(data->readAll());
    file.close();

    return true;
}

bool DownloadManager::isHttpRedirect(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return statusCode == 301 || statusCode == 302 || statusCode == 303
            || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

QString DownloadManager::getHref(QIODevice *data)
{
    QByteArray raw = data->readAll();
    QJsonDocument jdocument = QJsonDocument::fromJson(raw);
    QJsonObject jobj = jdocument.object();
    QString href = jobj["href"].toString();
    qDebug()<<href;
    return href;

}

void DownloadManager::sslErrors(const QList<QSslError> &sslErrors)
{
#if QT_CONFIG(ssl)
    for (const QSslError &error : sslErrors)
        fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
#else
    Q_UNUSED(sslErrors);
#endif
}

void DownloadManager::emitProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal == -1 || !bytesTotal)
        return;
    qint16 progress = int(((double(bytesReceived)/double(bytesTotal)) * 100));
    DownloadActionsPtr p = actions[reinterpret_cast<QNetworkReply*>(sender())];
    if (!p.isNull()) {
        p->progress(progress);
    }
}

void DownloadManager::downloadFinished(QNetworkReply *reply)
{

    QUrl url = reply->url();
    if (reply->error()) {
        fprintf(stderr, "Download of %s failed: %s\n",
                url.toEncoded().constData(),
                qPrintable(reply->errorString()));
    } else {
        if (isHttpRedirect(reply)) {
            qDebug()<<"Request was redirected.";
            doDownload(QUrl(reply->rawHeader("Location")), BINFILE, actions[reply]);
        } else {
            if (type[reply] == YDISK) {
                QString href = getHref(reply);
                doDownload(QUrl(href), BINFILE, actions[reply]);
            }
            else if (type[reply] == JSON) {
                QByteArray raw = reply->readAll();
                QJsonDocument jdocument = QJsonDocument::fromJson(raw);
                actions[reply]->jsonFineshed(jdocument);
            }
            else if (type[reply] == BINFILE) {
                QString filename = saveFileName(url);
                if (saveToDisk(filename, reply)) {
                    actions[reply]->fileFineshed(filename);
                    printf("Download of %s succeeded (saved to %s)\n",
                           url.toEncoded().constData(), qPrintable(filename));
                }
            }

        }
    }
    actions.remove(reply);
    currentDownloads.removeAll(reply);
    reply->deleteLater();
}
