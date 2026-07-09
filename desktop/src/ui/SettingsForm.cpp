#include "SettingsForm.h"

#include <algorithm>

#include "shell/Shell.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"

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
  return std::filesystem::exists(AppSettings::GetBaseDir() / "LOG_DEBUG");
}

void SettingsForm::SetDebugLoggingEnabled(bool enabled) {
  if(enabled) {
    if(!Shell::CreateFile(AppSettings::GetBaseDir() / "LOG_DEBUG"))
      spdlog::error("Failed to create debug log file.");
  } else {
    if(!Shell::RemoveFile(AppSettings::GetBaseDir() / "LOG_DEBUG"))
      spdlog::error("Failed to remove debug log file.");
  }
}

QString SettingsForm::GetOperatingSystem() {
  return QString::fromUtf8(AppInfo::GetOperatingSystem());
}

static bool IsUDPMethod(PairingMethod method) {
  return method == PairingMethod::UDP || method == PairingMethod::MANUAL_UDP;
}

bool SettingsForm::HasUDPDevices() {
  return std::ranges::any_of(PairedDevicesStorage::GetDevices(), [](PairedDevice &device) { return IsUDPMethod(device.pairingMethod); });
}

void SettingsForm::RemoveUDPDevices() {
  auto devices = PairedDevicesStorage::GetDevices();
  std::erase_if(devices, [](const PairedDevice &device) { return IsUDPMethod(device.pairingMethod); });
  PairedDevicesStorage::SaveDevices(devices);
}

void SettingsForm::Show(QObject *viewLoader) {
  m_EditSettings = AppSettingsModel(AppSettings::Get());
  m_EditServiceSettings = ServiceInstaller().GetSettings();
  QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/forms/SettingsForm.qml")));
}

void SettingsForm::OnSaveSettingsClicked(QObject *viewLoader, QObject *window) {
  AppSettings::Save(m_EditSettings.ToStorage());
  try {
    auto installer = ServiceInstaller();
    installer.ApplySettings(m_EditServiceSettings, false);
  } catch(const std::exception &ex) {
    spdlog::error("{}", ex.what());
    QMetaObject::invokeMethod(window, "showErrorMessage", Q_ARG(QVariant, "Failed to write service settings."));
  }
  AppSettings::InvalidateCache();
  QMetaObject::invokeMethod(viewLoader, "setSource", Q_ARG(QUrl, QUrl("qrc:/ui/forms/MainForm.qml")));
}
