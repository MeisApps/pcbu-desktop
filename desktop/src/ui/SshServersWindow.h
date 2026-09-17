#ifndef PCBU_DESKTOP_SSHSERVERSWINDOW_H
#define PCBU_DESKTOP_SSHSERVERSWINDOW_H

#include <QUrl>
#include <QtQmlIntegration>
#include <atomic>
#include <memory>
#include <thread>

#include "handler/UnlockHandler.h"

class SshServersWindow : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  ~SshServersWindow() override;

  Q_INVOKABLE QUrl GetSshDirUrl();
  Q_INVOKABLE QString GetPathFromUrl(const QUrl &url);

  Q_INVOKABLE bool AddServer(QObject *dialog, const QString &deviceId, const QString &host, const QString &user, const QString &keyPath,
                             const QString &password);
  Q_INVOKABLE void CancelAdd();
  Q_INVOKABLE void RemoveServer(const QString &deviceId, int serverIndex);

private:
  std::thread m_UnlockThread{};
  std::unique_ptr<UnlockHandler> m_UnlockHandler{};
  std::atomic<bool> m_IsRunning{};
};

#endif // PCBU_DESKTOP_SSHSERVERSWINDOW_H
