#ifndef PCBU_DESKTOP_UNLOCKTESTWINDOW_H
#define PCBU_DESKTOP_UNLOCKTESTWINDOW_H

#include <QtQmlIntegration>

#include "handler/UnlockHandler.h"

class UnlockTestWindow : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  ~UnlockTestWindow() override;

  Q_INVOKABLE bool IsRunning();
  Q_INVOKABLE void StartUnlock(QObject *window, const QString& deviceId);
  Q_INVOKABLE void StopUnlock(QObject *window);

private:
  std::thread m_UnlockThread{};
  std::unique_ptr<UnlockHandler> m_UnlockHandler{};
  std::atomic<bool> m_IsRunning{};
};

#endif // PCBU_DESKTOP_UNLOCKTESTWINDOW_H
