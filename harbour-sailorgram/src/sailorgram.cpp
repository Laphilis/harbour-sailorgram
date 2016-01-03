#include "sailorgram.h"

const QString SailorGram::CONFIG_FOLDER = "telegram";
const QString SailorGram::PUBLIC_KEY_FILE = "server.pub";
const QString SailorGram::EMOJI_FOLDER = "emoji";

SailorGram::SailorGram(QObject *parent): QObject(parent), _telegram(NULL), _daemonized(false)
{
    QDir cfgdir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    cfgdir.mkpath(qApp->applicationName() + QDir::separator() + qApp->applicationName() + QDir::separator() + SailorGram::CONFIG_FOLDER);

    this->_heartbeat = new HeartBeat(this);
    this->_interface = new SailorgramInterface(this);

    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)), this, SLOT(onApplicationStateChanged(Qt::ApplicationState)));
    connect(this->_interface, SIGNAL(wakeUpRequested()), this, SLOT(onWakeUpRequested()));
    connect(this->_heartbeat, SIGNAL(connectedChanged()), this, SLOT(wakeSleep()), Qt::QueuedConnection);
    connect(this->_heartbeat, SIGNAL(connectedChanged()), this, SIGNAL(connectedChanged()), Qt::QueuedConnection);
    connect(this->_heartbeat, SIGNAL(intervalChanged()), this, SIGNAL(intervalChanged()));
}

bool SailorGram::keepRunning() const
{
    return !qApp->quitOnLastWindowClosed();
}

bool SailorGram::daemonized() const
{
    return qApp->applicationState() == Qt::ApplicationActive;
}

bool SailorGram::connected() const
{
    return this->_heartbeat->connected();
}

int SailorGram::interval() const
{
    return this->_heartbeat->interval();
}

void SailorGram::setInterval(int interval)
{
    this->_heartbeat->setInterval(interval);
}

QString SailorGram::emojiPath() const
{
    return qApp->applicationDirPath() + QDir::separator() + "../share/" + qApp->applicationName() + QDir::separator() + SailorGram::EMOJI_FOLDER + QDir::separator();
}

QString SailorGram::configPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QDir::separator() + SailorGram::CONFIG_FOLDER;
}

QString SailorGram::publicKey() const
{
    return qApp->applicationDirPath() + QDir::separator() + "../share/" + qApp->applicationName() + QDir::separator() + SailorGram::PUBLIC_KEY_FILE;
}

TelegramQml *SailorGram::telegram() const
{
    return this->_telegram;
}

void SailorGram::setTelegram(TelegramQml *telegram)
{
    if(this->_telegram == telegram)
        return;

    this->_telegram = telegram;
    this->_heartbeat->setTelegram(telegram);

    if(this->_telegram->connected())
        this->_heartbeat->start();
    else
        connect(this->_telegram, SIGNAL(connectedChanged()), this, SLOT(startHeartBeat()));

    emit telegramChanged();
}

void SailorGram::setKeepRunning(bool keep)
{
    if(keep == this->keepRunning())
        return;

    qApp->setQuitOnLastWindowClosed(!keep);
    emit keepRunningChanged();
}

void SailorGram::moveMediaTo(FileLocationObject *locationobj, const QString &destination)
{
    if(!locationobj)
        return;

    QString filename = locationobj->fileName();
    QUrl location(locationobj->download()->location());

    if(filename.isEmpty())
        filename = location.fileName();

    QString destpath = destination + QDir::separator() + filename;

    if(QFile::exists(destpath)) // Don't overwrite existing files
        return;

    QFile::copy(location.path(), destpath);
}

void SailorGram::onApplicationStateChanged(Qt::ApplicationState state)
{
    bool active = state == Qt::ApplicationActive;

    if((!this->_daemonized && active) || (this->_daemonized && !active))
        return;

    this->_daemonized = !active;
    emit daemonizedChanged();
}

void SailorGram::onWakeUpRequested()
{
    if(this->_daemonized)
    {
        this->_daemonized = false;
        emit daemonizedChanged();
    }

    emit wakeUpRequested();
}

bool SailorGram::fileIsPhoto(const QString &filepath)
{
    if(filepath.isEmpty())
        return false;

    QUrl url(filepath);
    QMimeType mime = this->_mimedb.mimeTypeForFile(url.path());
    return mime.isValid() && (mime.name().split("/")[0] == "image");
}

bool SailorGram::fileIsVideo(const QString &filepath)
{
    if(filepath.isEmpty())
        return false;

    QUrl url(filepath);
    QMimeType mime = this->_mimedb.mimeTypeForFile(url.path());
    return mime.isValid() && (mime.name().split("/")[0] == "video");
}

QString SailorGram::fileName(const QString &filepath)
{
    QUrl url(filepath);
    return url.fileName();
}

FileLocationObject* SailorGram::mediaLocation(MessageMediaObject *messagemediaobject)
{
    if(!this->_telegram)
        return NULL;

    FileLocationObject* locationobj = NULL;

    switch(messagemediaobject->classType())
    {
        case MessageMedia::typeMessageMediaAudio:
            locationobj = this->_telegram->locationOfAudio(messagemediaobject->audio());
            break;

        case MessageMedia::typeMessageMediaDocument:
            locationobj = this->_telegram->locationOfDocument(messagemediaobject->document());
            break;

        case MessageMedia::typeMessageMediaVideo:
            locationobj = this->_telegram->locationOfVideo(messagemediaobject->video());
            break;

        case MessageMedia::typeMessageMediaPhoto:
            locationobj = messagemediaobject->photo()->sizes()->last()->location();
            break;

        default:
            break;
    }

    return locationobj;
}

void SailorGram::moveMediaToDownloads(MessageMediaObject *messagemediaobject)
{
    if(!this->_telegram)
        return;

    FileLocationObject* locationobj = this->mediaLocation(messagemediaobject);
    this->moveMediaTo(locationobj, QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
}

void SailorGram::moveMediaToGallery(MessageMediaObject *messagemediaobject)
{
    if(!this->_telegram)
        return;

    QString type;
    FileLocationObject* locationobj = this->mediaLocation(messagemediaobject);

    if(messagemediaobject->classType() == MessageMedia::typeMessageMediaDocument)
    {
        QString mime = messagemediaobject->document()->mimeType();
        type = mime.split("/")[0];
    }

    if((messagemediaobject->classType() == MessageMedia::typeMessageMediaVideo) || (type == "video"))
        this->moveMediaTo(locationobj, QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    else if((messagemediaobject->classType() == MessageMedia::typeMessageMediaAudio) || (type == "audio"))
        this->moveMediaTo(locationobj, QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    else if((messagemediaobject->classType() == MessageMedia::typeMessageMediaPhoto) || (type == "image"))
        this->moveMediaTo(locationobj, QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    else
        this->moveMediaToDownloads(messagemediaobject); // Fallback to Downloads folder
}

void SailorGram::startHeartBeat()
{
    if(!this->_telegram->connected())
        return;

    this->_heartbeat->start();
}

void SailorGram::wakeSleep()
{
    this->_heartbeat->connected() ? this->_telegram->wake() : this->_telegram->sleep();
}
