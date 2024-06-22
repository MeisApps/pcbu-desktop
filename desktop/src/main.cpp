#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>

#include "storage/LoggingSystem.h"

int main(int argc, char *argv[]) {
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArray("Material"));
    qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", QByteArray("Dark"));
    qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", QByteArray("Dense"));
    qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", QByteArray("Red"));
    qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", QByteArray("Teal"));
    LoggingSystem::Init("desktop");

    QGuiApplication app(argc, argv);
    QGuiApplication::setWindowIcon(QIcon(":/res/icons/icon.png"));

    auto url = QUrl("qrc:/ui/MainWindow.qml");
    QQmlApplicationEngine engine{};
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) { if(!obj && url == objUrl) { QCoreApplication::exit(-1); }}, Qt::QueuedConnection);
    engine.load(url);

    auto result = QGuiApplication::exec();
    LoggingSystem::Destroy();
    return result;
}
