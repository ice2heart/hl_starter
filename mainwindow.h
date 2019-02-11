#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork>
#include <QMap>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

#include <QListWidgetItem>

namespace Ui {
class MainWindow;
}

enum TYPES {YDISK, JSON, BINFILE};

static const QString SETTINGS_NICK = "Nick";
static const QString SETTINGS_DIR = "Dirs";
static const QString SETTINGS_LAST_HOME = "Home_Key";
static const QString SETTINGS_AUTOSELECT = "Home_Autoselect";
static const QString SETTINGS_USER_ARGS = "User_args";
static const QString SETTINGS_START_HIDDEN = "Start_Hidden";
static const QString SETTINGS_STEAM_SELECTOR = "Steam_selector";

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
signals:
    void updated();
    void cleaned();
    void downloaded();
    void doDownload();
};
typedef QSharedPointer<Package> PackagePtr;

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

class MainWindow;

class PackageItem: public QWidget
{
    Q_OBJECT
    DownloadManager *m_dm;
    PackagePtr m_package;

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
public slots:
    void update();
    void clean();
    void download();
};

struct News
{
    QString header;
    QString text;
    QUrl link;
    QString mode;
    QDateTime date;
    QString args;

};
typedef QVector<News> NewsList;

class NewsWidget: public QWidget
{
    Q_OBJECT
    QGridLayout mainLayout;
    QLabel header;
    QLabel text;
    QPushButton runButton;
    QTimer event;
public:
    NewsWidget(QWidget *parent, const News & news);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QSettings* settings, QWidget *parent = nullptr);
    ~MainWindow();

    QString homeDirecotry;
    void updateInstalledPackages();
    void updateRemotePackageList();
public slots:
    void addFolder();
    void removeFolder();
    void getHomeFolder(QListWidgetItem *item);
    void selectedHomeFolder();
    void selectHome(const QString &home);
    void launchPackage(const QString &package, const QStringList &args = {});
    void downloadAndLaunch(const QString &package, const QStringList &args = {});
    void changeHomeDirectory();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void showNotify(const QString &header, const QString &text);
protected:
    void closeEvent(QCloseEvent * event);
private:
    QSettings *m_settings;
    Ui::MainWindow *ui;
    QSystemTrayIcon *trayIcon;
    DownloadManager m_manager;
    QString profile_name;

    QMap<QString, PackagePtr> packages;
    QMap<QString, QVariant> folders;
    QMap<QString, QVariant> steam_versions;
    NewsList news;

    void packagesIsLoaded();
    bool insertPackage(const PackagePtr &package);
    void redrawDirectory();
    void saveDirectorys();
    void updateNewsList();


};

#endif // MAINWINDOW_H
