#include "PairingForm.h"

#include "installer/service/ServiceInstaller.h"
#include "platform/BluetoothHelper.h"
#include "utils/I18n.h"
#include "utils/QRUtils.h"

PairingForm::~PairingForm() {
  m_Session.Stop();
  m_IsBluetoothScanRunning = false;
  if(m_BluetoothScanThread.joinable())
    m_BluetoothScanThread.join();
  if(m_BluetoothPairThread.joinable())
    m_BluetoothPairThread.join();
}

PairingAssistantModel PairingForm::GetData() {
  return m_PairingData;
}

void PairingForm::SetData(const PairingAssistantModel &data) {
  m_PairingData = data;
}

QUrl PairingForm::GetQRImage() {
  auto qrSvg = QRUtils::GenerateSVG(m_Session.GetPayload());
  qrSvg = "data:image/svg+xml;utf8," + qrSvg;
  return {QString::fromUtf8(qrSvg.c_str())};
}

QString PairingForm::GetPairingCode() {
  return QString::fromUtf8(m_Session.GetPairingCode());
}

bool PairingForm::HasBluetooth() {
  return BluetoothHelper::IsAvailable();
}

void PairingForm::ReportPairingSuccess(QObject *viewLoader, QObject *window) {
  QMetaObject::invokeMethod(this, "OnPairingSuccess", Qt::QueuedConnection, Q_ARG(QObject *, viewLoader), Q_ARG(QObject *, window));
}

void PairingForm::ReportPairingError(QObject *viewLoader, QObject *window, const std::string &error) {
  QMetaObject::invokeMethod(this, "OnPairingError", Qt::QueuedConnection, Q_ARG(QObject *, viewLoader), Q_ARG(QObject *, window),
                            Q_ARG(QString, QString::fromUtf8(error)));
}

void PairingForm::OnPairingSuccess(QObject *viewLoader, QObject *window) {
  if(m_CurrentStep == PairingStep::QR_SCAN)
    OnNextClicked(viewLoader, window);
}

void PairingForm::OnPairingError(QObject *viewLoader, QObject *window, const QString &error) {
  QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, error));
  if(m_CurrentStep == PairingStep::QR_SCAN)
    OnBackClicked(viewLoader, window);
}

void PairingForm::SetSkipPasswordCheck(bool skip) {
  m_SkipPasswordCheck = skip;
}

PairingStep PairingForm::GetNextStep() {
  auto nextStep = PairingStep::USER_PASSWORD_SELECT;
  switch(m_CurrentStep) {
    case PairingStep::USER_PASSWORD_SELECT:
      nextStep = PairingStep::DEVICE_TYPE_SELECT;
      break;
    case PairingStep::DEVICE_TYPE_SELECT:
      nextStep = PairingStep::METHOD_TYPE_SELECT;
      break;
    case PairingStep::METHOD_TYPE_SELECT:
      nextStep = m_PairingData.pairingMethodType == "CLOUD" ? PairingStep::QR_SCAN : PairingStep::METHOD_SELECT;
      break;
    case PairingStep::METHOD_SELECT: {
      auto method = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
      if(method == PairingMethod::BLUETOOTH)
        nextStep = PairingStep::BLUETOOTH_DEVICE_SELECT;
      else
        nextStep = PairingStep::QR_SCAN;
      break;
    }
    case PairingStep::BLUETOOTH_DEVICE_SELECT:
      nextStep = PairingStep::BLUETOOTH_PAIRING;
      break;
    case PairingStep::BLUETOOTH_PAIRING:
      nextStep = PairingStep::QR_SCAN;
      break;
    case PairingStep::QR_SCAN:
      nextStep = PairingStep::NONE;
      break;
    default:
      break;
  }
  return nextStep;
}

void PairingForm::UpdateStepForm(QObject *viewLoader, QObject *window) {
  // Set pairing method
  if(m_PairingData.deviceType == "IOS" && m_PairingData.pairingMethodType == "AUTO")
    m_PairingData.pairingMethodType = "CLOUD";
  auto selectedMethod = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
  if(m_PairingData.pairingMethodType == "CLOUD") {
    if(selectedMethod != PairingMethod::CLOUD)
      m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(PairingMethod::CLOUD));
  } else if(m_PairingData.pairingMethodType == "MANUAL") {
    if(selectedMethod != PairingMethod::MANUAL_UDP)
      m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(PairingMethod::MANUAL_UDP));
  } else if(selectedMethod == PairingMethod::CLOUD || selectedMethod == PairingMethod::MANUAL_UDP) {
    m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(PairingMethod::UDP));
  }

  // Pairing server
  if(m_CurrentStep == PairingStep::QR_SCAN) {
    auto successCallback = [this, viewLoader, window]() { ReportPairingSuccess(viewLoader, window); };
    auto errorCallback = [this, viewLoader, window](const std::string &error) { ReportPairingError(viewLoader, window, error); };
    auto uiData = PairingUIData();
    uiData.userName = m_PairingData.userName.toStdString();
    uiData.password = m_PairingData.password.toStdString();
    uiData.method = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
    uiData.btAddress = m_PairingData.bluetoothAddress.toStdString();
    uiData.useLegacy = m_PairingData.useLegacyPairing;
    m_Session.Start(uiData, successCallback, errorCallback);
  } else {
    m_Session.Stop();
  }

  // Bluetooth scanner
  if(m_CurrentStep == PairingStep::BLUETOOTH_DEVICE_SELECT) {
    if(m_BluetoothScanThread.joinable())
      m_BluetoothScanThread.join();
    m_IsBluetoothScanRunning = true;
    m_BluetoothScanThread = std::thread([this, window]() {
      BluetoothHelper::StartScan();
      while(m_IsBluetoothScanRunning) {
        auto devices = BluetoothHelper::ScanDevices();
        if(!m_IsBluetoothScanRunning)
          break;

        spdlog::info("Bluetooth: {} devices.", devices.size());
        QVariantList uiDevices{};
        for(const auto &device : devices) {
          BluetoothDeviceModel entry = {QString::fromUtf8(device.name), QString::fromUtf8(device.address)};
          uiDevices.append(QVariant::fromValue(entry));
        }
        QMetaObject::invokeMethod(window, "updateBluetoothDeviceList", Q_ARG(QVariant, uiDevices));
      }
      BluetoothHelper::StopScan();
    });
  } else {
    m_IsBluetoothScanRunning = false;
  }

  // Bluetooth pairing
  if(m_CurrentStep == PairingStep::BLUETOOTH_PAIRING) {
    auto device = BluetoothDevice();
    device.address = m_PairingData.bluetoothAddress.toStdString();
    if(m_BluetoothPairThread.joinable())
      m_BluetoothPairThread.join();
    m_BluetoothPairThread = std::thread([window, device]() {
      auto result = BluetoothHelper::PairDevice(device);
      QMetaObject::invokeMethod(window, "finishBluetoothPairing", Q_ARG(QVariant, result));
    });
  }

  switch(m_CurrentStep) {
    case PairingStep::USER_PASSWORD_SELECT:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingPasswordForm.qml"));
      break;
    case PairingStep::DEVICE_TYPE_SELECT:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingDeviceTypeForm.qml"));
      break;
    case PairingStep::METHOD_TYPE_SELECT:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingMethodTypeForm.qml"));
      break;
    case PairingStep::METHOD_SELECT:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingMethodForm.qml"));
      break;
    case PairingStep::BLUETOOTH_DEVICE_SELECT:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingBTSelectForm.qml"));
      break;
    case PairingStep::BLUETOOTH_PAIRING:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingBTPairForm.qml"));
      break;
    case PairingStep::QR_SCAN:
      viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingQRForm.qml"));
      break;
    default:
      viewLoader->setProperty("source", QUrl("qrc:/ui/forms/MainForm.qml"));
      break;
  }
}

void PairingForm::Show(QObject *viewLoader, QObject *window) {
  if(!ServiceInstaller::IsInstalled()) {
    QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_pairing_not_installed"))));
    return;
  }

  m_PairingData.deviceType = "ANDROID";
  m_PairingData.pairingMethodType = "CLOUD";
  m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(PairingMethod::CLOUD));
  m_PairingData.useLegacyPairing = false;
  m_CurrentStep = PairingStep::USER_PASSWORD_SELECT;
  m_StepStack = {};
  m_SkipPasswordCheck = false;
  viewLoader->setProperty("source", QUrl("qrc:/ui/pairing/PairingPasswordForm.qml"));
}

void PairingForm::OnBackClicked(QObject *viewLoader, QObject *window) {
  if(m_StepStack.empty()) {
    viewLoader->setProperty("source", QUrl("qrc:/ui/forms/MainForm.qml"));
    return;
  }

  m_CurrentStep = m_StepStack.top();
  m_StepStack.pop();
  UpdateStepForm(viewLoader, window);
}

void PairingForm::OnNextClicked(QObject *viewLoader, QObject *window) {
  if(m_CurrentStep == PairingStep::USER_PASSWORD_SELECT) {
    auto skipPasswordCheck = m_SkipPasswordCheck;
    m_SkipPasswordCheck = false;
    auto check = PairingSession::CheckCredentials(m_PairingData.userName.toStdString(), m_PairingData.password.toStdString(),
                                                  m_PairingData.isManualUserName, skipPasswordCheck);
    if(!check.error.empty()) {
      if(check.canSkip) {
        QMetaObject::invokeMethod(window, "showSkipPasswordCheckMessage", Q_ARG(QVariant, QString::fromUtf8(check.error)));
        return;
      }
      QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(check.error)));
      return;
    }
  }

  auto nextStep = GetNextStep();
  if(m_CurrentStep != PairingStep::NONE && m_CurrentStep != PairingStep::BLUETOOTH_PAIRING)
    m_StepStack.push(m_CurrentStep);
  m_CurrentStep = nextStep;
  UpdateStepForm(viewLoader, window);
}
