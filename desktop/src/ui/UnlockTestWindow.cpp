#include "UnlockTestWindow.h"

UnlockTestWindow::~UnlockTestWindow() {
  m_IsRunning.store(false);
  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
}

bool UnlockTestWindow::IsRunning() {
  return m_IsRunning;
}

void UnlockTestWindow::StartUnlock(QObject *window, const QString& deviceId) {
  auto device = PairedDevicesStorage::GetDeviceByID(deviceId.toStdString());
  if(!device.has_value()) {
    QMetaObject::invokeMethod(window, "close");
    return;
  }

  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
  m_UnlockHandler.reset();
  m_UnlockHandler = std::make_unique<UnlockHandler>([window](const std::string &s) {
    QMetaObject::invokeMethod(window, "setUnlockMessage", Q_ARG(QVariant, QString::fromUtf8(s)));
  });

  m_IsRunning.store(true);
  m_UnlockThread = std::thread([this, window, device]() {
    QMetaObject::invokeMethod(window, "setUnlockButtonText", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("cancel"))));
    m_UnlockHandler->GetResult(device.value().userName, I18n::Get("unlock_test"), &m_IsRunning);
    QMetaObject::invokeMethod(window, "setUnlockButtonText", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("retry"))));
    m_IsRunning.store(false);
  });
}

void UnlockTestWindow::StopUnlock(QObject *window) {
  m_IsRunning.store(false);
  if(m_UnlockThread.joinable())
    m_UnlockThread.join();
  QMetaObject::invokeMethod(window, "setUnlockMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("unlock_canceled"))));
}
