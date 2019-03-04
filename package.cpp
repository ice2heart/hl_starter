#include "package.h"

#include <QMessageBox>

PackageItem::PackageItem(QWidget *parent, DownloadManager *dm, PackagePtr p)
    :QWidget(parent)
    ,m_dm(dm)
    ,m_package(p)
{
    downLoadButton.setText(tr("Download"));
    playButton.setText(tr("Play"));
    deleteButton.setText(tr("Delete"));
    progress.setVisible(false);
    layout.addWidget(&label);
    layout.addWidget(&progress);
    layout.addWidget(&downLoadButton);
    layout.addWidget(&playButton);
    layout.addWidget(&deleteButton);
    setLayout(&layout);
    update();
    mv = reinterpret_cast<MainWindow*>(parent);
    connect(m_package.data(), &Package::updated, this, &PackageItem::update);
    connect(m_package.data(), &Package::cleaned, this, &PackageItem::clean);
    connect(&playButton, &QPushButton::clicked, [=](){
        mv->launchPackage(m_package->name);
    });
    connect(&deleteButton, &QPushButton::clicked, [=]() {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Delete directory", tr("Are you sure to delete?"),
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }
        QDir dir(mv->homeDirecotry);
        dir.cd(m_package->name);
        dir.removeRecursively();
        m_package->isLocal = false;
        update();
        mv->updateInstalledPackages();
    });
    connect(&downLoadButton, &QPushButton::clicked, this, &PackageItem::download);
    connect(m_package.data(), &Package::doDownload, this, &PackageItem::download);
}

void PackageItem::download()
{
    if (m_package->isLocal){
        emit m_package->downloaded();
        return;
    }
    progress.setVisible(true);
    curentDownload = m_dm->doDownload(m_package->href);
    connect(curentDownload.data(), &DownloadActions::fileFineshed, [=](const QString &filename) {
        progress.setVisible(false);
        QDir().rename(filename, QDir(mv->homeDirecotry).filePath(filename));
        //rename to p.name+zip
        //7za.exe x filename
        //move to homedir
        const QString program = "7za.exe";
        const QStringList arguments = QStringList() << "x" << filename;
        QProcess *process = new QProcess;

        process->setWorkingDirectory(mv->homeDirecotry);
        process->start(program, arguments);
        connect(process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                [=](int, QProcess::ExitStatus){
            mv->updateInstalledPackages();
            process->deleteLater();
            QDir(mv->homeDirecotry).remove(filename);
            emit m_package->downloaded();
        });
        curentDownload.reset();
    });
    connect(curentDownload.data(), &DownloadActions::progress, [=](int progressVal) {
        progress.setValue(progressVal);
    });
}

void PackageItem::update()
{
    if (!m_package->liblist.isEmpty()) {
        QString lbl("%0 (%1)");
        lbl = lbl.arg(m_package->liblist, m_package->name);
        label.setText(lbl);
    } else {
        label.setText(m_package->name);
    }

    if (!m_package->isLocal) {
        label.setStyleSheet("QLabel { color : grey; }");
        playButton.hide();
        deleteButton.hide();
        downLoadButton.show();
        progress.setValue(0);
    } else {
        label.setStyleSheet("QLabel { color : black; }");
        progress.hide();
        downLoadButton.hide();
        playButton.show();
        deleteButton.show();
    }
}

void PackageItem::clean()
{
    if (!curentDownload.isNull()) {
        qDebug()<<"canceling download";
        curentDownload->cancel();
        curentDownload.reset();
    }
    m_package.clear();
}

Package::~Package()
{
    emit cleaned();
}
