#include "PairingForm.h"

#include "installer/ServiceInstaller.h"
#include "platform/PlatformHelper.h"
#include "platform/BluetoothHelper.h"
#include "storage/AppSettings.h"
#include "utils/QRUtils.h"
#include "utils/StringUtils.h"

PairingForm::~PairingForm() {
    m_IsBluetoothScanRunning = false;
    if(m_BluetoothScanThread.joinable())
        m_BluetoothScanThread.join();
    if(m_BluetoothPairThread.joinable())
        m_BluetoothPairThread.join();
}

PairingAssistantModel PairingForm::GetData() {
    return m_PairingData;
}

void PairingForm::SetData(const PairingAssistantModel& data) {
    m_PairingData = data;
}

QUrl PairingForm::GetQRImage() {
    auto qrData = PairingQRData(NetworkHelper::GetSavedNetworkInterface().ipAddress,
                                AppSettings::Get().pairingServerPort,
                                PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString()),
                                m_EncKey);
    auto qrSvg = QRUtils::GenerateSVG(qrData.ToJson().dump());
    qrSvg = "data:image/svg+xml;utf8," + qrSvg;
    return {QString::fromUtf8(qrSvg.c_str())};
}

QString PairingForm::GetPairingCode() {
    auto qrStr = PairingQRData(NetworkHelper::GetSavedNetworkInterface().ipAddress,
                                AppSettings::Get().pairingServerPort,
                                PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString()),
                                m_EncKey).ToJson().dump();
    auto codeStr = StringUtils::WithSeperators(StringUtils::ToBase32String(qrStr), "-", 5);
    return QString::fromUtf8(codeStr);
}

bool PairingForm::HasBluetooth() {
    return BluetoothHelper::IsAvailable();
}

PairingStep PairingForm::GetNextStep() {
    auto nextStep = PairingStep::USER_PASSWORD_SELECT;
    switch (m_CurrentStep) {
        case PairingStep::USER_PASSWORD_SELECT:
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
    if(m_PairingServer) {
        m_PairingServer->Stop();
        if(m_CurrentStep == PairingStep::QR_SCAN) {
            m_EncKey = StringUtils::RandomString(64);
            auto serverData = PairingServerData();
            serverData.userName = m_PairingData.userName.toStdString();
            serverData.password = m_PairingData.password.toStdString();
            serverData.encKey = m_EncKey;
            serverData.method = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
            serverData.macAddress = NetworkHelper::GetSavedNetworkInterface().macAddress;
            serverData.btAddress = m_PairingData.bluetoothAddress.toStdString();
            m_PairingServer->Start(serverData);
        } else {
            m_EncKey = {};
        }
    }

    // Bluetooth scanner
    if(m_CurrentStep == PairingStep::BLUETOOTH_DEVICE_SELECT) {
        if(m_BluetoothScanThread.joinable())
            m_BluetoothScanThread.join();
        m_IsBluetoothScanRunning = true;
        m_BluetoothScanThread = std::thread([this, window]() {
            BluetoothHelper::StartScan();
            while (m_IsBluetoothScanRunning) {
                auto devices = BluetoothHelper::ScanDevices();
                if(!m_IsBluetoothScanRunning)
                    break;

                spdlog::info("Bluetooth: {} devices.", devices.size());
                QVariantList uiDevices{};
                for(const auto& device : devices) {
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

    switch (m_CurrentStep) {
        case PairingStep::USER_PASSWORD_SELECT:
            QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/pairing/PairingPasswordForm.qml")));
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
    try {
        m_PairingServer.reset();
        m_PairingServer = std::make_unique<PairingServer>();
    } catch(const std::exception& ex) {
        spdlog::error("Error initializing pairing server: {}", ex.what());
        QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_server_init"))));
        return;
    }

    m_PairingData.pairingMethod = QString::fromUtf8(PairingMethodUtils::ToString(PairingMethod::TCP));
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
        switch (loginResult) {
            case PlatformLoginResult::INVALID_USER:
                errorStr = I18n::Get("error_invalid_user");
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
