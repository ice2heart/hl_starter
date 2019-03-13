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
#include "download.h"

class Package;
typedef QSharedPointer<Package> PackagePtr;
class PackageItem;

namespace Ui {
class MainWindow;
}



static const QString SETTINGS_NICK = "Nick";
static const QString SETTINGS_DIR = "Dirs";
static const QString SETTINGS_LAST_HOME = "Home_Key";
static const QString SETTINGS_AUTOSELECT = "Home_Autoselect";
static const QString SETTINGS_USER_ARGS = "User_args";
static const QString SETTINGS_START_HIDDEN = "Start_Hidden";
static const QString SETTINGS_STEAM_SELECTOR = "Steam_selector";

static const QString VERSION = "0.0.1";


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
    void update_app(const QString &url);
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
    QMap<QString, QVariant> mod_args;
    PackageItem *curent_package;
    NewsList news;

    void packagesIsLoaded();
    bool insertPackage(const PackagePtr &package);
    void redrawDirectory();
    void saveDirectorys();
    void updateNewsList();


};

#endif // MAINWINDOW_H
