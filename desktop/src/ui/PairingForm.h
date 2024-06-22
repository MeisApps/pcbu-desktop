#ifndef PCBU_DESKTOP_PAIRINGFORM_H
#define PCBU_DESKTOP_PAIRINGFORM_H

#include <stack>
#include <QObject>
#include <QtQmlIntegration>

#include "connection/servers/PairingServer.h"
#include "platform/NetworkHelper.h"

enum class PairingStep {
    USER_PASSWORD_SELECT,
    METHOD_SELECT,
    BLUETOOTH_DEVICE_SELECT,
    BLUETOOTH_PAIRING,
    QR_SCAN,
    NONE
};

struct PairingAssistantModel {
    Q_GADGET
public:
    QString userName{};
    QString password{};
    QString pairingMethod{};
    QString bluetoothAddress{};
    Q_PROPERTY(QString userName MEMBER userName)
    Q_PROPERTY(QString password MEMBER password)
    Q_PROPERTY(QString pairingMethod MEMBER pairingMethod)
    Q_PROPERTY(QString bluetoothAddress MEMBER bluetoothAddress)
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
    Q_INVOKABLE void SetData(const PairingAssistantModel& data);

    Q_INVOKABLE QUrl GetQRImage();
    Q_INVOKABLE bool HasBluetooth();

public slots:
    void OnBackClicked(QObject *viewLoader, QObject *window);
    void OnNextClicked(QObject *viewLoader, QObject *window);
    void Show(QObject *viewLoader, QObject *window);

private:
    PairingStep GetNextStep();
    void UpdateStepForm(QObject *viewLoader, QObject *window);
    NetworkInterface GetNetworkInterface();

    PairingStep m_CurrentStep{};
    std::stack<PairingStep> m_StepStack{};

    std::string m_EncKey{};
    PairingAssistantModel m_PairingData{};

    bool m_IsBluetoothScanRunning{};
    std::thread m_BluetoothScanThread{};
    std::thread m_BluetoothPairThread{};
    std::unique_ptr<PairingServer> m_PairingServer = std::make_unique<PairingServer>();
};

#endif //PCBU_DESKTOP_PAIRINGFORM_H
