#ifndef PACKAGE_H
#define PACKAGE_H
#include "mainwindow.h"

class Package: public QObject
{
    Q_OBJECT
public:
    Package() = default;
    ~Package();
    QString name;
    QUrl href;
    QString version;
    bool isLocal;
    QString args;
    QString startArgs;
    QString liblist;
signals:
    void updated();
    void cleaned();
    void downloaded();
    void doDownload();
};

class PackageItem: public QWidget
{
    Q_OBJECT
    DownloadManager *m_dm;
    QHBoxLayout layout;
    QLabel label;
    QPushButton downLoadButton;
    QPushButton playButton;
    QPushButton deleteButton;
    QProgressBar progress;
    DownloadActionsPtr curentDownload;
    MainWindow *mv;
public:
    PackageItem(QWidget *parent, DownloadManager *dm, PackagePtr p);
    PackagePtr m_package;
public slots:
    void update();
    void clean();
    void download();
};
#endif // PACKAGE_H
