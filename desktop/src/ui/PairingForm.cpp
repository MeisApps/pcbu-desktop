#include "PairingForm.h"

#include "installer/ServiceInstaller.h"
#include "platform/PlatformHelper.h"
#include "platform/BluetoothHelper.h"
#include "storage/AppSettings.h"
#include "utils/StringUtils.h"
#include "utils/QRUtils.h"

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
    auto qrData = PairingQRData(GetNetworkInterface().ipAddress,
                                AppSettings::Get().serverPort,
                                PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString()),
                                m_EncKey);
    auto qrSvg = QRUtils::GenerateSVG(qrData.ToJson().dump());
    qrSvg = "data:image/svg+xml;utf8," + qrSvg;
    return {QString::fromUtf8(qrSvg.c_str())};
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
    if(m_PairingServer)
        m_PairingServer->Stop();
    if(m_CurrentStep == PairingStep::QR_SCAN) {
        m_EncKey = StringUtils::RandomString(64);
        auto serverData = PairingServerData();
        serverData.userName = m_PairingData.userName.toStdString();
        serverData.password = m_PairingData.password.toStdString();
        serverData.encKey = m_EncKey;
        serverData.method = PairingMethodUtils::FromString(m_PairingData.pairingMethod.toStdString());
        serverData.macAddress = GetNetworkInterface().macAddress;
        serverData.btAddress = m_PairingData.bluetoothAddress.toStdString();
        m_PairingServer->Start(serverData);
    } else {
        m_EncKey = {};
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

NetworkInterface PairingForm::GetNetworkInterface() {
    NetworkInterface result{};
    auto localIfs = NetworkHelper::GetLocalNetInterfaces();
    auto settings = AppSettings::Get();
    auto found = false;
    if(settings.serverIP == "auto") {
        for(const auto& netIf : localIfs) {
            if(netIf.macAddress == settings.serverMAC) {
                result = netIf;
                found = true;
                break;
            }
        }
        if(!found) {
            if(!localIfs.empty())
                result = localIfs[0];
            spdlog::warn("Invalid server IP settings.");
        }
    } else {
        result.ipAddress = settings.serverIP;
    }
    return result;
}

void PairingForm::Show(QObject *viewLoader, QObject *window) {
    if(!ServiceInstaller::IsInstalled()) {
        QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_pairing_not_installed"))));
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
        if(!PlatformHelper::HasUserPassword(m_PairingData.userName.toStdString())) {
            QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_user_no_password"))));
            return;
        }
        if(!PlatformHelper::CheckLogin(m_PairingData.userName.toStdString(), m_PairingData.password.toStdString())) {
            QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error_incorrect_password"))));
            return;
        }
    }

    auto nextStep = GetNextStep();
    if(m_CurrentStep != PairingStep::NONE && m_CurrentStep != PairingStep::BLUETOOTH_PAIRING)
        m_StepStack.push(m_CurrentStep);
    m_CurrentStep = nextStep;
    UpdateStepForm(viewLoader, window);
}
