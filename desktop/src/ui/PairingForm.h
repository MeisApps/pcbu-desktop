#ifndef PCBU_DESKTOP_PAIRINGFORM_H
#define PCBU_DESKTOP_PAIRINGFORM_H

#include <QtQmlIntegration>
#include <stack>
#include <thread>

#include "pairing/PairingSession.h"

enum class PairingStep { USER_PASSWORD_SELECT, METHOD_TYPE_SELECT, METHOD_SELECT, BLUETOOTH_DEVICE_SELECT, BLUETOOTH_PAIRING, QR_SCAN, NONE };

struct PairingAssistantModel {
  Q_GADGET
public:
  QString userName{};
  QString password{};
  bool isManualUserName{};
  QString pairingMethodType{};
  QString pairingMethod{};
  QString bluetoothAddress{};
  bool useLegacyPairing{};
  Q_PROPERTY(QString userName MEMBER userName)
  Q_PROPERTY(QString password MEMBER password)
  Q_PROPERTY(bool isManualUserName MEMBER isManualUserName)
  Q_PROPERTY(QString pairingMethodType MEMBER pairingMethodType)
  Q_PROPERTY(QString pairingMethod MEMBER pairingMethod)
  Q_PROPERTY(QString bluetoothAddress MEMBER bluetoothAddress)
  Q_PROPERTY(bool useLegacyPairing MEMBER useLegacyPairing)
};

struct BluetoothDeviceModel {
  Q_GADGET
public:
  QString name{};
  QString address{};
  Q_PROPERTY(QString name MEMBER name)
  Q_PROPERTY(QString address MEMBER address)
};

class PairingForm : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
public:
  ~PairingForm() override;

  Q_INVOKABLE PairingAssistantModel GetData();
  Q_INVOKABLE void SetData(const PairingAssistantModel &data);

  Q_INVOKABLE QUrl GetQRImage();
  Q_INVOKABLE QString GetPairingCode();
  Q_INVOKABLE bool HasBluetooth();
  Q_INVOKABLE void SetSkipPasswordCheck(bool skip);

public slots:
  void OnPairingSuccess(QObject *viewLoader, QObject *window);
  void OnPairingError(QObject *viewLoader, QObject *window, const QString &error);

  void OnBackClicked(QObject *viewLoader, QObject *window);
  void OnNextClicked(QObject *viewLoader, QObject *window);

  void Show(QObject *viewLoader, QObject *window);

private:
  PairingStep GetNextStep();
  void UpdateStepForm(QObject *viewLoader, QObject *window);

  void ReportPairingSuccess(QObject *viewLoader, QObject *window);
  void ReportPairingError(QObject *viewLoader, QObject *window, const std::string &error);

  PairingStep m_CurrentStep{};
  std::stack<PairingStep> m_StepStack{};

  PairingAssistantModel m_PairingData{};
  bool m_SkipPasswordCheck{};

  bool m_IsBluetoothScanRunning{};
  std::thread m_BluetoothScanThread{};
  std::thread m_BluetoothPairThread{};

  PairingSession m_Session{};
};

#endif // PCBU_DESKTOP_PAIRINGFORM_H
