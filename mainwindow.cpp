#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QJsonDocument>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QDir>
#include <QStringList>
#include <QProcess>
#include <QMenu>
#include <QCloseEvent>
#include <QApplication>
#include <QRegExp>

#include <QPushButton>
#include <QMessageBox>
#include <QDesktopServices>

#include <QDebug>

#include "package.h"

MainWindow::MainWindow(QSettings *settings, QWidget *parent) :
    QMainWindow(parent),
    m_settings(settings),
    ui(new Ui::MainWindow),
    trayIcon(new QSystemTrayIcon(this))
{
    ui->setupUi(this);
    curent_package = nullptr;
    trayIcon->setIcon(this->style()->standardIcon(QStyle::SP_ComputerIcon));
    trayIcon->setToolTip(tr("Game launcher"));
    QMenu * menu = new QMenu(this);
    QAction * viewWindow = new QAction(tr("Open window"), this);
    QAction * quitAction = new QAction(tr("Exit"), this);
    connect(viewWindow, &QAction::triggered, [=](){
        this->showNormal();

    });
    connect(quitAction, &QAction::triggered, [=](){
        QApplication::quit();
    });

    menu->addAction(viewWindow);
    menu->addAction(quitAction);
    trayIcon->setContextMenu(menu);
    trayIcon->show();
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));

    ui->nickLineEdit->setText(m_settings->value(SETTINGS_NICK, "Player1").toString());
    folders = m_settings->value(SETTINGS_DIR).toMap();
    steam_versions = m_settings->value(SETTINGS_STEAM_SELECTOR).toMap();
    bool autoopen = m_settings->value(SETTINGS_AUTOSELECT, false).toBool();
    redrawDirectory();
    ui->mainPages->setCurrentIndex(0);
    ui->autoHomeCheckBox->setChecked(autoopen);
//    ui->argsLineEdit->setText(m_settings->value(SETTINGS_USER_ARGS, "").toString());
    mod_args = m_settings->value(SETTINGS_USER_ARGS).toMap();
    connect(ui->AddFolderButton, SIGNAL(clicked()), this, SLOT(addFolder()));
    connect(ui->RemoveFolderButton, SIGNAL(clicked()), this, SLOT(removeFolder()));
    connect(ui->home_selection, &QPushButton::clicked, this, &MainWindow::selectedHomeFolder);
    connect(ui->foldersListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::getHomeFolder);
    connect(ui->changeHomeButton, &QPushButton::clicked, this, &MainWindow::changeHomeDirectory);
    connect(ui->nickLineEdit, &QLineEdit::textChanged, [=](const QString &text){
        m_settings->setValue(SETTINGS_NICK, text);
    });
    connect(ui->argsLineEdit, &QLineEdit::textChanged, [=](const QString &text){
        if (!curent_package) {
            QMessageBox msgBox;
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(tr("Please, select mode before"));
            msgBox.exec();
            return;
        }
        mod_args[curent_package->m_package->name] = ui->argsLineEdit->text();
        m_settings->setValue(SETTINGS_USER_ARGS, mod_args);
    });
    connect(ui->autoHomeCheckBox, &QCheckBox::clicked, [=](bool checked) {
        m_settings->setValue(SETTINGS_AUTOSELECT, checked);
    });
    connect(ui->settingsButton, &QPushButton::clicked, [=](){
        ui->mainPages->setCurrentIndex(2);
    });
    connect(ui->backSettingsPushButton, &QPushButton::clicked, [=](){
        ui->mainPages->setCurrentIndex(1);
    });
    connect(ui->openGameFolderButton, &QPushButton::clicked, [=](){
       QDesktopServices::openUrl(QUrl::fromLocalFile(homeDirecotry));
    });
    connect(ui->listAvalible, &QListWidget::itemClicked, [=](QListWidgetItem* item){
      curent_package = static_cast<PackageItem*>(ui->listAvalible->itemWidget(item));
      if (!curent_package)
          return;
      ui->argsLineEdit->setText(mod_args[curent_package->m_package->name].toString());
    });
    updateRemotePackageList();
    QString last_home = m_settings->value(SETTINGS_LAST_HOME, "").toString();
    if (autoopen && !last_home.isEmpty()){
        QString home = folders[last_home].toString();
        if (!home.isEmpty()) {
            selectHome(last_home);
        }
    }

    bool startHidden = m_settings->value(SETTINGS_START_HIDDEN, false).toBool();
    ui->startinMinimizedCheckBox->setChecked(startHidden);
    connect(ui->startinMinimizedCheckBox, &QCheckBox::clicked, [=](bool checked) {
        m_settings->setValue(SETTINGS_START_HIDDEN, checked);
    });
    if (!startHidden){
        this->show();
    }

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateInstalledPackages()
{
    //    QStringList exclude;
    //    exclude << "config"<< "gldrv"<< "platform"<< "reslists"<< "valve";
    QDir home(this->homeDirecotry);
    QStringList topDirs = home.entryList( QDir::Dirs|QDir::NoDotAndDotDot);
    for (QString modDir: topDirs) {
        QDir localDir(home.filePath( modDir));
        QStringList gameinfo = localDir.entryList(QDir::Files);
        QStringList filtred;
        filtred << gameinfo.filter("liblist.gam") << gameinfo.filter("gameinfo.json");
        if (filtred.isEmpty())
            continue;
//        qDebug() << filtred<<modDir;
        PackagePtr p(new Package);
        if (filtred.contains("liblist.gam")){
            QFile file(QDir(home.filePath(modDir)).filePath("liblist.gam"));
            if (!file.open(QIODevice::ReadOnly)) {
                fprintf(stderr, "Could not open for reading: %s\n",
                        qPrintable(file.errorString()));
            }
            else {
                QString list(file.readAll());
                file.close();
                QRegExp rx("^game\\s+\\\"([^\\\"]+)\\\"");
                if (rx.indexIn(list) != -1){
                    p->liblist = rx.cap(1);
                    qDebug()<<p->liblist;
                }
            }
        }
        p->name = modDir;
        p->isLocal = true;
        if (modDir == "valve"){
            continue;
        }
        bool inserted = this->insertPackage(p);
        if (inserted)
            continue;
        PackagePtr plocal = this->packages[modDir];
        plocal->isLocal = true;
        emit plocal->updated();

    }

}

void MainWindow::addFolder()
{
    QString direcotry = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "",
                                                      QFileDialog::ShowDirsOnly
                                                      | QFileDialog::DontResolveSymlinks);
    if (direcotry.isEmpty())
        return;
    bool ok;
    QString text = QInputDialog::getText(this, tr("Name of this game folder"),
                                         tr("Name"), QLineEdit::Normal,
                                         direcotry, &ok);
    if (!ok)
        return;
    if (text.isEmpty())
        text = direcotry;

    //check
    QDir home(direcotry);
    QStringList files = home.entryList( QDir::Files|QDir::NoDotAndDotDot);
    if (!files.contains("hl.exe", Qt::CaseInsensitive)){
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("Please, select directory with hl.exe"));
        msgBox.exec();
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Steam or not", "Is it a steam version?",
                                    QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        steam_versions[text] = true;
    }

    folders[text] = direcotry;
    redrawDirectory();
    saveDirectorys();
}

void MainWindow::removeFolder()
{
    int index = ui->foldersListWidget->currentRow();
    if (index == -1)
        return;
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete directory", tr("Are you sure to delete?"),
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::No) {
        return;
    }
    QString item = ui->foldersListWidget->currentItem()->text();
    folders.remove(item);
    saveDirectorys();
    redrawDirectory();
}

void MainWindow::getHomeFolder(QListWidgetItem *item)
{
    selectHome(item->text());
}

void MainWindow::selectedHomeFolder()
{
    int index = ui->foldersListWidget->currentRow();
    if (index == -1)
        return;
    QString item = ui->foldersListWidget->currentItem()->text();
    selectHome(item);
}

void MainWindow::selectHome(const QString &home)
{
    m_settings->setValue(SETTINGS_LAST_HOME, home);
    profile_name = home;
    ui->profileLable->setText(home);
    homeDirecotry = folders[home].toString();
//    QDir home_d(this->homeDirecotry);
//    QStringList topFiles = home_d.entryList( QDir::Files|QDir::NoDotAndDotDot);
//    if (topFiles.contains("steam_api.dll", Qt::CaseInsensitive)){
//        qDebug()<<"steam version";
//    }
    updateInstalledPackages();
    packagesIsLoaded();
}

void MainWindow::launchPackage(const QString &package, const QStringList &args)
{
    QStringList fullargs ={"-game", package, "+name", ui->nickLineEdit->text()};
    QStringList uiArgs = ui->argsLineEdit->text().split(" ");
    fullargs = fullargs + args + uiArgs;
    QDir exePath(homeDirecotry);
    qDebug()<<fullargs;
    if (steam_versions[profile_name].toBool()){
        qDebug()<<"Steam version";
        QDesktopServices::openUrl(QUrl("steam://rungameid/70//" + fullargs.join(" ")));
    } else {
        qDebug()<<"Normal version";
        QProcess::startDetached(exePath.filePath("hl.exe"), fullargs, homeDirecotry);
    }


}

void MainWindow::downloadAndLaunch(const QString &package, const QStringList &args)
{
    if (package == "valve") {
        this->launchPackage(package, args);
    }
    if (!packages.contains(package)){
        return;
    }
    PackagePtr p = packages[package];
    connect(p.data(), &Package::downloaded, [=](){
        this->launchPackage(package, args);
    });
    emit p->doDownload();
}

void MainWindow::changeHomeDirectory()
{
    homeDirecotry = "";
    ui->mainPages->setCurrentIndex(0);
    //clear packages
    // stop downloading
    for(const auto &p: packages){
        emit p->cleaned();
    }
    packages.clear();
    ui->listAvalible->clear();
    news.clear();
    for (auto i : ui->newsListWidget->findItems("", Qt::MatchContains)){
        qDebug()<<__FUNCTION__<<i->text();
        //i->deleteLater();
        delete i;
    }
    ui->newsListWidget->clear();
    updateRemotePackageList();
    //get list of packages
    //download list of packages

}

void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason){
    case QSystemTrayIcon::Trigger:
        this->raise();
        this->showNormal();
        this->setFocus();
        QTimer::singleShot(0, this, SLOT(showNormal()));

        break;
    default:
        break;
    }

}

void MainWindow::showNotify(const QString &header, const QString &text)
{
    trayIcon->showMessage(header, text);

}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(this->isVisible()){
        event->ignore();
        this->hide();
        return;
    }
}

void MainWindow::packagesIsLoaded()
{
    if (!homeDirecotry.isEmpty() && !packages.isEmpty()) {
        ui->mainPages->setCurrentIndex(1);
    }
}

bool MainWindow::insertPackage(const PackagePtr &package)
{
    if (this->packages.contains(package->name))
        return false;
    this->packages[package->name] = package;
    qDebug()<<package->href<<package->name;

    QListWidgetItem *listWidgetItem = new QListWidgetItem(this->ui->listAvalible);
    ui->listAvalible->addItem(listWidgetItem);
    PackageItem *pi = new PackageItem(this, &m_manager, package);
    listWidgetItem->setSizeHint(pi->sizeHint());
    ui->listAvalible->setItemWidget(listWidgetItem, pi);
    return true;

}

void MainWindow::redrawDirectory()
{
    ui->foldersListWidget->clear();
    for(auto a: folders.keys()){
        ui->foldersListWidget->addItem(a);
    }
}

void MainWindow::saveDirectorys()
{
    m_settings->setValue(SETTINGS_DIR, folders);
    m_settings->setValue(SETTINGS_STEAM_SELECTOR, steam_versions);
}

void MainWindow::updateNewsList()
{
    for (const News& n: news){
        //ui->newsListWidget->addItem();
        qDebug()<<n.date.toLocalTime()<<n.header<<n.text;
        QListWidgetItem *listWidgetItem = new QListWidgetItem(this->ui->newsListWidget);
        ui->listAvalible->addItem(listWidgetItem);
        NewsWidget *newsWidget = new NewsWidget(this, n);
        listWidgetItem->setSizeHint(newsWidget->sizeHint());
        ui->newsListWidget->setItemWidget(listWidgetItem, newsWidget);
    }

}

void MainWindow::updateRemotePackageList()
{
    DownloadActionsPtr r = m_manager.doDownloadJSON(QUrl("https://ice2heart.com/packagelist.json"));
    connect(r.data(), &DownloadActions::jsonFineshed, [=]( const QJsonDocument &jdoc){
        QJsonObject main = jdoc.object();
        QJsonArray jpackages = main["list"].toArray();
        QJsonArray jnews = main["news"].toArray();
        for (auto jpackage : jpackages) {
            QJsonObject jobj = jpackage.toObject();
            PackagePtr p(new Package);

            p->name = jobj["name"].toString();
            p->href = QUrl(jobj["href"].toString());
            p->version = jobj["version"].toString();
            p->args = jobj["args"].toString();
            p->isLocal = false;
            if (!this->insertPackage(p)) {
                PackagePtr plocal = this->packages[p->name];
                plocal->href = QUrl(jobj["href"].toString());
                plocal->version = jobj["version"].toString();
                emit plocal->updated();
            }
        }
        for (auto i: jnews) {
            News n;
            QJsonObject jitem  = i.toObject();
            n.header = jitem["header"].toString();
            n.text = jitem["text"].toString();
            n.mode = jitem["mode"].toString();
            n.link = jitem["link"].toString();
            n.date = jitem["date"].toVariant().toDateTime();
            n.args = jitem["args"].toString();
            this->news.append(n);
        }
        this->updateNewsList();
        this->packagesIsLoaded();
    });
}

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


NewsWidget::NewsWidget(QWidget *parent, const News &news)
    :QWidget (parent)
{
    MainWindow *p = static_cast<MainWindow*>(parent);
    connect(&runButton, &QPushButton::clicked, [=](){
        qDebug()<<"launch!"<<news.mode<<news.args.split(" ");
        p->downloadAndLaunch(news.mode, news.args.split(" "));
    });
    header.setText(news.header);
    header.setStyleSheet("font-weight: bold;");
    QString textLine("%0<br>");
    textLine = textLine.arg(news.text);
    text.setTextFormat(Qt::RichText);
    text.setTextInteractionFlags(Qt::TextBrowserInteraction);
    text.setOpenExternalLinks(true);
    text.setWordWrap(true);
    if (news.date.isValid()){
        textLine += tr("Date: %0<br>").arg(news.date.toString("dd.MM.yyyy hh:mm"));
        QDateTime now = QDateTime::currentDateTime();
        qint64 diff = news.date.toMSecsSinceEpoch() - now.toMSecsSinceEpoch();
        if (diff > 0) {
            event.setInterval((int)diff);
            event.setSingleShot(true);
            event.start();
            connect(&event, &QTimer::timeout, [=](){
                p->showNotify(tr("Time has come"), news.header);
            });
        }
    }
    if (news.link.isValid()){
        textLine += tr("Link: <a href=\"%2\">Details!</a><br>").arg(news.link.toString());
    }
    textLine += "<br>";
    text.setText(textLine);
    runButton.setText("Download and run");
    setLayout(&mainLayout);
    mainLayout.addWidget(&header, 0,0);
    mainLayout.addWidget(&text,1,0);
    if (news.mode.length()) {
        mainLayout.addWidget(&runButton,1,1);
    }
}
