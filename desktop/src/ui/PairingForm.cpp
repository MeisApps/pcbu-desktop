#include "PairingForm.h"

#include "installer/ServiceInstaller.h"
#include "platform/BluetoothHelper.h"
#include "platform/NetworkHelper.h"
#include "platform/PlatformHelper.h"
#include "storage/AppSettings.h"
#include "utils/QRUtils.h"
#include "utils/StringUtils.h"

PairingForm::~PairingForm() {
  m_IsBluetoothScanRunning = false;
  if(m_BluetoothScanThread.joinable())
    m_BluetoothScanThread.join();
  if(m_BluetoothPairThread.joinable())
    m_BluetoothPairThread.join();
  if(m_PairingServer)
    m_PairingServer->Stop();
  if(m_DiscoveryBeacon)
    m_DiscoveryBeacon->Stop();
}

PairingAssistantModel PairingForm::GetData() {
  return m_PairingData;
}

void PairingForm::SetData(const PairingAssistantModel &data) {
  m_PairingData = data;
}

QUrl PairingForm::GetQRImage() {
  auto qrSvg = QRUtils::GenerateSVG(BuildPairingPayload());
  qrSvg = "data:image/svg+xml;utf8," + qrSvg;
  return {QString::fromUtf8(qrSvg.c_str())};
}

QString PairingForm::GetPairingCode() {
  auto codeStr = StringUtils::WithSeperators(StringUtils::ToBase32String(BuildPairingPayload()), "-", 5);
  return QString::fromUtf8(codeStr);
}

std::string PairingForm::BuildPairingPayload() {
  auto settings = AppSettings::Get();
  auto method = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
  if(m_PairingData.useLegacyPairing) {
    return LegacyPairingQRData(NetworkHelper::GetSavedNetworkInterface().ipAddress, settings.pairingServerPort, method, m_EncKey).ToJson().dump();
  }
  return PairingQRData(m_ServerId, settings.pairingDiscoveryPort, method, m_EncKey).ToJson().dump();
}

bool PairingForm::HasBluetooth() {
  return BluetoothHelper::IsAvailable();
}

PairingStep PairingForm::GetNextStep() {
  auto nextStep = PairingStep::USER_PASSWORD_SELECT;
  switch(m_CurrentStep) {
    case PairingStep::USER_PASSWORD_SELECT:
      nextStep = PairingStep::METHOD_TYPE_SELECT;
      break;
    case PairingStep::METHOD_TYPE_SELECT:
      nextStep = PairingStep::METHOD_SELECT;
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
  // Pairing server
  if(m_CurrentStep == PairingStep::QR_SCAN) {
    if(m_PairingServer) {
      m_PairingServer->Stop();
    }
    m_PairingServer.reset();
    if(m_DiscoveryBeacon) {
      m_DiscoveryBeacon->Stop();
      m_DiscoveryBeacon.reset();
    }
    m_PairingServer = std::make_unique<PairingServer>(
        [window](const std::string &error) { QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(error))); });

    m_ServerId = StringUtils::RandomString(8);
    m_EncKey = StringUtils::RandomString(64);
    auto uiData = PairingUIData();
    uiData.userName = m_PairingData.userName.toStdString();
    uiData.password = m_PairingData.password.toStdString();
    uiData.encKey = m_EncKey;
    uiData.method = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
    uiData.macAddress = NetworkHelper::GetSavedNetworkInterface().macAddress;
    uiData.btAddress = m_PairingData.bluetoothAddress.toStdString();
    uiData.useLegacy = m_PairingData.useLegacyPairing;
    m_PairingServer->Start(uiData);

    if(!m_PairingData.useLegacyPairing) {
      m_DiscoveryBeacon = std::make_unique<UDPPairingBroadcaster>(m_ServerId, m_EncKey);
      m_DiscoveryBeacon->Start();
    }
  } else {
    m_ServerId = {};
    m_EncKey = {};
    if(m_PairingServer) {
      m_PairingServer->Stop();
    }
    if(m_DiscoveryBeacon) {
      m_DiscoveryBeacon->Stop();
      m_DiscoveryBeacon.reset();
    }
  }

  // Set default pairing method
  if(m_CurrentStep == PairingStep::METHOD_SELECT) {
    auto defaultMethod = m_PairingData.pairingMethodType == "AUTO" ? PairingMethod::UDP : PairingMethod::MANUAL_UDP;
    m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(defaultMethod));
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
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingPasswordForm.qml")));
      break;
    case PairingStep::METHOD_TYPE_SELECT:
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingMethodTypeForm.qml")));
      break;
    case PairingStep::METHOD_SELECT:
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingMethodForm.qml")));
      break;
    case PairingStep::BLUETOOTH_DEVICE_SELECT:
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingBTSelectForm.qml")));
      break;
    case PairingStep::BLUETOOTH_PAIRING:
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingBTPairForm.qml")));
      break;
    case PairingStep::QR_SCAN:
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingQRForm.qml")));
      break;
    default:
      QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/forms/MainForm.qml")));
      break;
  }
}

void PairingForm::Show(QObject *viewLoader, QObject *window) {
  if(!ServiceInstaller::IsInstalled()) {
    QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_pairing_not_installed"))));
    return;
  }

  m_PairingData.pairingMethodType = "AUTO";
  m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(PairingMethod::UDP));
  m_PairingData.useLegacyPairing = false;
  m_CurrentStep = PairingStep::USER_PASSWORD_SELECT;
  m_StepStack = {};
  QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingPasswordForm.qml")));
}

void PairingForm::OnBackClicked(QObject *viewLoader, QObject *window) {
  if(m_StepStack.empty()) {
    QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/forms/MainForm.qml")));
    return;
  }

  m_CurrentStep = m_StepStack.top();
  m_StepStack.pop();
  UpdateStepForm(viewLoader, window);
}

void PairingForm::OnNextClicked(QObject *viewLoader, QObject *window) {
  if(m_CurrentStep == PairingStep::USER_PASSWORD_SELECT) {
#ifdef WINDOWS
    if(StringUtils::Split(m_PairingData.userName.toStdString(), "\\").size() != 2) {
      QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_win_invalid_user"))));
      return;
    }
#endif
    if(!PlatformHelper::HasUserPassword(m_PairingData.userName.toStdString())) {
      auto errorStr = m_PairingData.isManualUserName ? I18n::Get("error_invalid_user_or_no_password") : I18n::Get("error_user_no_password");
      QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(errorStr)));
      return;
    }
    auto loginResult = PlatformHelper::CheckLogin(m_PairingData.userName.toStdString(), m_PairingData.password.toStdString());
    std::string errorStr{};
    switch(loginResult) {
      case PlatformLoginResult::INVALID_USER:
        errorStr = I18n::Get("error_invalid_user_input");
        break;
      case PlatformLoginResult::INVALID_PASSWORD:
        errorStr = m_PairingData.isManualUserName ? I18n::Get("error_invalid_user_or_incorrect_password") : I18n::Get("error_incorrect_password");
        break;
      case PlatformLoginResult::ACCOUNT_LOCKED:
        errorStr = I18n::Get("error_win_account_locked");
        break;
      case PlatformLoginResult::SUCCESS:
        break;
    }
    if(!errorStr.empty()) {
      QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(errorStr)));
      return;
    }
  }

  auto nextStep = GetNextStep();
  if(m_CurrentStep != PairingStep::NONE && m_CurrentStep != PairingStep::BLUETOOTH_PAIRING)
    m_StepStack.push(m_CurrentStep);
  m_CurrentStep = nextStep;
  UpdateStepForm(viewLoader, window);
}
