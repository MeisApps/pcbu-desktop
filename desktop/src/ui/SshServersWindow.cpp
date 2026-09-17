#include "SshServersWindow.h"

#include <QDir>

#include "storage/PairedDevicesStorage.h"
#include "utils/CryptUtils.h"
#include "utils/I18n.h"
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
    auto result = m_UnlockHandler->GetResult(device->userName, "Add server (SSH)", {device->id}, &m_IsRunning);
    auto ok = false;
    auto error = QString();
    if(result.state != UnlockState::SUCCESS) {
      error = QString::fromUtf8(UnlockStateUtils::ToString(result.state));
    } else if(result.device.id != device->id || result.passwordKey.empty()) {
      error = QString::fromUtf8(I18n::Get("error_ssh_wrong_device"));
    } else {
      auto enc = CryptUtils::EncryptAES(passStr, result.passwordKey);
      auto newDevice = PairedDevicesStorage::GetDeviceByID(device->id);
      if(!enc.has_value()) {
        error = QString::fromUtf8(I18n::Get("error_ssh_encrypt"));
      } else if(!newDevice.has_value()) {
        error = QString::fromUtf8(I18n::Get("error_unknown"));
      } else {
        auto server = SshServer();
        server.host = hostStr;
        server.user = userStr;
        server.keyPath = keyStr;
        server.passwordEnc = enc.value();
        newDevice->sshServers.emplace_back(server);
        PairedDevicesStorage::AddDevice(newDevice.value());
        ok = true;
      }
    }
    m_IsRunning.store(false);
    QMetaObject::invokeMethod(dialog, "finishAdd", Q_ARG(QVariant, ok), Q_ARG(QVariant, error));
  });
  return true;
}

void SshServersWindow::CancelAdd() {
  m_IsRunning.store(false);
  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
}

void SshServersWindow::RemoveServer(const QString &deviceId, int serverIndex) {
  auto device = PairedDevicesStorage::GetDeviceByID(deviceId.toStdString());
  if(!device.has_value() || serverIndex < 0 || serverIndex >= (int)device->sshServers.size())
    return;
  device->sshServers.erase(device->sshServers.begin() + serverIndex);
  PairedDevicesStorage::AddDevice(device.value());
}
