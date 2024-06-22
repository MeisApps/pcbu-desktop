#ifndef PCBU_DESKTOP_SETTINGSFORM_H
#define PCBU_DESKTOP_SETTINGSFORM_H

#include <QObject>
#include <QtQmlIntegration>

#include "storage/AppSettings.h"
#include "installer/ServiceInstaller.h"

struct AppSettingsModel {
Q_GADGET
public:
    AppSettingsModel() = default;
    explicit AppSettingsModel(const PCBUAppStorage& settings) {
        language = QString::fromUtf8(settings.language);
        serverIP = QString::fromUtf8(settings.serverIP);
        serverMAC = QString::fromUtf8(settings.serverMAC);
        serverPort = settings.serverPort;
        waitForKeyPress = settings.waitForKeyPress;
    }
    [[nodiscard]] PCBUAppStorage ToStorage() const {
        auto settings = PCBUAppStorage();
        settings.language = language.toStdString();
        settings.serverIP = serverIP.toStdString();
        settings.serverMAC = serverMAC.toStdString();
        settings.serverPort = serverPort;
        settings.waitForKeyPress = waitForKeyPress;
        return settings;
    }

    QString language{};
    QString serverIP{};
    QString serverMAC{};
    uint16_t serverPort{};
    bool waitForKeyPress{};
    Q_PROPERTY(QString language MEMBER language)
    Q_PROPERTY(QString serverIP MEMBER serverIP)
    Q_PROPERTY(QString serverMAC MEMBER serverMAC)
    Q_PROPERTY(uint16_t serverPort MEMBER serverPort)
    Q_PROPERTY(bool waitForKeyPress MEMBER waitForKeyPress)
};

struct ServiceSettingModel {
Q_GADGET
public:
    QString id{};
    QString name{};
    bool enabled{};
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(bool enabled MEMBER enabled)
};

class SettingsForm : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    Q_INVOKABLE AppSettingsModel GetSettings();
    Q_INVOKABLE void SetSettings(const AppSettingsModel& settings);

    Q_INVOKABLE QVariantList GetServiceSettings();
    Q_INVOKABLE void SetServiceSettings(const QVariantList& settings);

    Q_INVOKABLE QString GetOperatingSystem();

public slots:
    void Show(QObject *viewLoader);
    void OnSaveSettingsClicked(QObject *viewLoader);

private:
    AppSettingsModel m_EditSettings{};
    std::vector<ServiceSetting> m_EditServiceSettings{};
};

#endif //PCBU_DESKTOP_SETTINGSFORM_H
