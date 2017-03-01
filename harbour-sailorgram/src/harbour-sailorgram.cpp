#include <QtQuick>
#include <sailfishapp.h>
#include <autogenerated/telegramqml.h>
#include "dbus/screenblank.h"
#include "selector/audiorecorder.h"
#include "selector/thumbnailprovider.h"
#include "selector/filesmodel.h"
#include "model/dialogscovermodel.h"
#include "sailorgram.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(SailfishApp::application(argc, argv));
    application->setApplicationName("harbour-sailorgram");
    application->setApplicationVersion("0.9");
//    application->addLibraryPath(QString("%1/../share/%2/lib").arg(qApp->applicationDirPath(), qApp->applicationName()));

    QStringList args = application->arguments();
    bool daemonized = args.contains("-daemon");

//    if(daemonized && !SailorGram::hasDaemonFile()) { // segfault on using SailorGram::hasDaemonFile
//        return 0;
//    }

    QDBusConnection sessionbus = QDBusConnection::sessionBus();

    if(sessionbus.interface()->isServiceRegistered(SailorgramInterface::INTERFACE_NAME)) // Only a Single Instance is allowed
    {
        SailorgramInterface::sendWakeUp();

        if(application->hasPendingEvents())
            application->processEvents();

        return 0;
    }

    FilesModel::registerMetaTypes();
    TelegramQml::initialize("harbour.sailorgram.LibQTelegram");

    qmlRegisterType<SailorGram>("harbour.sailorgram.SailorGram", 1, 0, "SailorGram");
    qmlRegisterType<ScreenBlank>("harbour.sailorgram.DBus", 1, 0, "ScreenBlank");
    qmlRegisterType<AudioRecorder>("harbour.sailorgram.Selector", 1, 0, "AudioRecorder");
    qmlRegisterType<FilesModel>("harbour.sailorgram.FilesModel", 1, 0, "FilesModel");
    qmlRegisterType<DialogsCoverModel>("harbour.sailorgram.Model", 1, 0, "DialogsCoverModel");
    qmlRegisterType<TranslationInfoItem>("harbour.sailorgram.Model", 1, 0, "TranslationItem");

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    QQmlEngine* engine = view->engine();
    QObject::connect(engine, SIGNAL(quit()), application.data(), SLOT(quit()));
    engine->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider);
    view->rootContext()->setContextProperty("quickView", view.data());

    view->setSource(SailfishApp::pathTo("qml/harbour-sailorgram.qml"));

    if(daemonized) {
        view->create();
    } else {
        view->show();
    }

    return application->exec();
}
