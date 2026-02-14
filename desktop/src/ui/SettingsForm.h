#ifndef PCBU_DESKTOP_SETTINGSFORM_H
#define PCBU_DESKTOP_SETTINGSFORM_H

#include <QObject>
#include <QtQmlIntegration>

#include "installer/ServiceInstaller.h"
#include "storage/AppSettings.h"

struct AppSettingsModel {
  Q_GADGET
public:
  AppSettingsModel() = default;
  explicit AppSettingsModel(const PCBUAppStorage &settings) {
    machineID = QString::fromStdString(settings.machineID);
    installedVersion = QString::fromUtf8(settings.installedVersion);
    language = QString::fromUtf8(settings.language);
    serverIP = QString::fromUtf8(settings.serverIP);
    serverMAC = QString::fromUtf8(settings.serverMAC);
    pairingServerPort = settings.pairingServerPort;
    unlockServerPort = settings.unlockServerPort;
    clientSocketTimeout = settings.clientSocketTimeout;
    clientConnectTimeout = settings.clientConnectTimeout;
    clientConnectRetries = settings.clientConnectRetries;
    winWaitForKeyPress = settings.winWaitForKeyPress;
    winHidePasswordField = settings.winHidePasswordField;
  }
  [[nodiscard]] PCBUAppStorage ToStorage() const {
    auto settings = PCBUAppStorage();
    settings.machineID = machineID.toStdString();
    settings.installedVersion = installedVersion.toStdString();
    settings.language = language.toStdString();
    settings.serverIP = serverIP.toStdString();
    settings.serverMAC = serverMAC.toStdString();
    settings.pairingServerPort = pairingServerPort;
    settings.unlockServerPort = unlockServerPort;
    settings.clientSocketTimeout = clientSocketTimeout;
    settings.clientConnectTimeout = clientConnectTimeout;
    settings.clientConnectRetries = clientConnectRetries;
    settings.winWaitForKeyPress = winWaitForKeyPress;
    settings.winHidePasswordField = winHidePasswordField;
    return settings;
  }

  QString machineID{};
  QString installedVersion{};
  QString language{};
  QString serverIP{};
  QString serverMAC{};
  uint16_t pairingServerPort{};
  uint16_t unlockServerPort{};
  uint32_t clientSocketTimeout{};
  uint32_t clientConnectTimeout{};
  uint32_t clientConnectRetries{};
  bool winWaitForKeyPress{};
  bool winHidePasswordField{};
  Q_PROPERTY(QString machineID MEMBER machineID)
  Q_PROPERTY(QString installedVersion MEMBER installedVersion)
  Q_PROPERTY(QString language MEMBER language)
  Q_PROPERTY(QString serverIP MEMBER serverIP)
  Q_PROPERTY(QString serverMAC MEMBER serverMAC)
  Q_PROPERTY(uint16_t pairingServerPort MEMBER pairingServerPort)
  Q_PROPERTY(uint16_t unlockServerPort MEMBER unlockServerPort)
  Q_PROPERTY(uint32_t clientSocketTimeout MEMBER clientSocketTimeout)
  Q_PROPERTY(uint32_t clientConnectTimeout MEMBER clientConnectTimeout)
  Q_PROPERTY(uint32_t clientConnectRetries MEMBER clientConnectRetries)
  Q_PROPERTY(bool winWaitForKeyPress MEMBER winWaitForKeyPress)
  Q_PROPERTY(bool winHidePasswordField MEMBER winHidePasswordField)
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
  Q_INVOKABLE void SetSettings(const AppSettingsModel &settings);

  Q_INVOKABLE QVariantList GetServiceSettings();
  Q_INVOKABLE void SetServiceSettings(const QVariantList &settings);

  Q_INVOKABLE bool IsDebugLoggingEnabled();
  Q_INVOKABLE void SetDebugLoggingEnabled(bool enabled);

  Q_INVOKABLE QString GetOperatingSystem();

public slots:
  void Show(QObject *viewLoader);
  void OnSaveSettingsClicked(QObject *viewLoader, QObject *window);

private:
  AppSettingsModel m_EditSettings{};
  std::vector<ServiceSetting> m_EditServiceSettings{};
};

#endif // PCBU_DESKTOP_SETTINGSFORM_H
