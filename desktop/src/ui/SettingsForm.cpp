#include "SettingsForm.h"

#include "storage/LoggingSystem.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"

SettingsForm::~SettingsForm() {
  if(m_SaveThread.joinable())
    m_SaveThread.join();
}

AppSettingsModel SettingsForm::GetSettings() {
  return m_EditSettings;
}

void SettingsForm::SetSettings(const AppSettingsModel &settings) {
  m_EditSettings = settings;
}

QVariantList SettingsForm::GetServiceSettings() {
  QVariantList result{};
  for(const auto &setting : m_EditServiceSettings) {
    ServiceSettingModel entry{};
    entry.id = QString::fromUtf8(setting.id);
    entry.name = QString::fromUtf8(setting.name);
    entry.enabled = setting.enabled;
    entry.type = QString::fromUtf8(setting.type);
    entry.selectedValue = QString::fromUtf8(setting.selectedValue);
    for(const auto &option : setting.options) {
      entry.optionNames.append(QString::fromUtf8(option.name));
      entry.optionValues.append(QString::fromUtf8(option.value));
    }
    result.append(QVariant::fromValue(entry));
  }
  return result;
}

void SettingsForm::SetServiceSettings(const QVariantList &settings) {
  for(const auto &varSetting : settings) {
    auto model = varSetting.value<ServiceSettingModel>();
    auto id = model.id.toStdString();
    for(auto &setting : m_EditServiceSettings) {
      if(setting.id != id)
        continue;
      setting.enabled = model.enabled;
      setting.selectedValue = model.selectedValue.toStdString();
      break;
    }
  }
}

bool SettingsForm::IsDebugLoggingEnabled() {
  return LoggingSystem::IsDebugEnabled();
}

void SettingsForm::SetDebugLoggingEnabled(bool enabled) {
  if(!LoggingSystem::SetDebugEnabled(enabled))
    spdlog::error(enabled ? "Failed to create debug log file." : "Failed to remove debug log file.");
}

QString SettingsForm::GetOperatingSystem() {
  return QString::fromUtf8(AppInfo::GetOperatingSystem());
}

bool SettingsForm::HasUDPDevices() {
  return PairedDevicesStorage::HasDevicesWithMethod(PairingMethod::UDP) || PairedDevicesStorage::HasDevicesWithMethod(PairingMethod::MANUAL_UDP);
}

void SettingsForm::RemoveUDPDevices() {
  PairedDevicesStorage::RemoveDevicesWithMethod(PairingMethod::UDP);
  PairedDevicesStorage::RemoveDevicesWithMethod(PairingMethod::MANUAL_UDP);
}

void SettingsForm::Show(QObject *viewLoader) {
  m_EditSettings = AppSettingsModel(AppSettings::Get());
  m_EditServiceSettings = ServiceInstaller().GetSettings();
  viewLoader->setProperty("source", QUrl("qrc:/ui/forms/SettingsForm.qml"));
}

void SettingsForm::OnSaveSettingsClicked(QObject *viewLoader, QObject *window) {
  AppSettings::Save(m_EditSettings.ToStorage());
  if(m_SaveThread.joinable())
    m_SaveThread.join();
  QMetaObject::invokeMethod(window, "showLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("please_wait"))));
  m_SaveThread = std::thread([viewLoader, window, serviceSettings = m_EditServiceSettings]() {
    auto logCallback = [window](const std::string &str) {
      spdlog::info(str);
      QMetaObject::invokeMethod(window, "appendLoadingOutput", Q_ARG(QVariant, QString::fromUtf8(str)));
    };
    try {
      auto installer = ServiceInstaller(logCallback);
      installer.ApplySettings(serviceSettings, false);
    } catch(const std::exception &ex) {
      logCallback(ex.what());
      QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("error"))));
      return;
    }
    AppSettings::InvalidateCache();
    QMetaObject::invokeMethod(window, "finishLoadingScreen", Q_ARG(QVariant, QString::fromUtf8(I18n::Get("success"))));
    QMetaObject::invokeMethod(viewLoader, [viewLoader]() { viewLoader->setProperty("source", QUrl("qrc:/ui/forms/MainForm.qml")); });
  });
}
