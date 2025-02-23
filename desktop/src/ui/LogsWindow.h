#ifndef PCBU_DESKTOP_LOGSWINDOW_H
#define PCBU_DESKTOP_LOGSWINDOW_H

#include <QObject>
#include <QtQmlIntegration>

class LogsWindow : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
public:
  ~LogsWindow() override;

  Q_INVOKABLE void LoadLogs(QObject *window);

private:
  std::thread m_LoadThread{};
};

#endif // PCBU_DESKTOP_LOGSWINDOW_H
