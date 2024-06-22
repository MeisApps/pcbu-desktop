#ifndef PCBU_DESKTOP_LOGSWINDOW_H
#define PCBU_DESKTOP_LOGSWINDOW_H

#include <QObject>
#include <QtQmlIntegration>

class LogsWindow : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    Q_INVOKABLE QString GetDesktopLogs();
    Q_INVOKABLE QString GetModuleLogs();
};

#endif //PCBU_DESKTOP_LOGSWINDOW_H
