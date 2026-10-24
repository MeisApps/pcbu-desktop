#include "SshServersWindow.h"

#include <QDir>

#include "ssh/SshServerHelper.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/StringUtils.h"

SshServersWindow::~SshServersWindow() {
  m_IsRunning.store(false);
  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
}

QUrl SshServersWindow::GetSshDirUrl() {
  return QUrl::fromLocalFile(QDir::homePath() + "/.ssh");
}

QString SshServersWindow::GetPathFromUrl(const QUrl &url) {
  return url.toLocalFile();
}

bool SshServersWindow::AddServer(QObject *dialog, const QString &deviceId, const QString &host, const QString &user, const QString &keyPath,
                                 const QString &password) {
  if(m_IsRunning)
    return false;
  auto hostStr = StringUtils::Trim(host.toStdString());
  auto userStr = StringUtils::Trim(user.toStdString());
  auto keyStr = StringUtils::Trim(keyPath.toStdString());
  auto passStr = password.toStdString();
  if((hostStr.empty() && keyStr.empty()) || passStr.empty())
    return false;
  auto device = PairedDevicesStorage::GetDeviceByID(deviceId.toStdString());
  if(!device.has_value())
    return false;

  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
  m_UnlockHandler.reset();
  m_UnlockHandler = std::make_unique<UnlockHandler>(
      [dialog](const std::string &s) { QMetaObject::invokeMethod(dialog, "setUnlockMessage", Q_ARG(QVariant, QString::fromUtf8(s))); });

  m_IsRunning.store(true);
  m_UnlockThread = std::thread([this, dialog, device, hostStr, userStr, keyStr, passStr]() {
    auto server = SshServer();
    server.host = hostStr;
    server.user = userStr;
    server.keyPath = keyStr;
    auto error = SshServerHelper::AddServer(*m_UnlockHandler, device.value(), server, passStr, &m_IsRunning);
    m_IsRunning.store(false);
    QMetaObject::invokeMethod(dialog, "finishAdd", Q_ARG(QVariant, error.empty()), Q_ARG(QVariant, QString::fromUtf8(error)));
  });
  return true;
}

void SshServersWindow::CancelAdd() {
  m_IsRunning.store(false);
  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
}

void SshServersWindow::RemoveServer(const QString &deviceId, int serverIndex) {
  SshServerHelper::RemoveServer(deviceId.toStdString(), serverIndex);
}
