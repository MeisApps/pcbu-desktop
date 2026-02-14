#include "SettingsForm.h"

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
    ServiceSettingModel entry = {QString::fromUtf8(setting.id), QString::fromUtf8(setting.name), setting.enabled};
    result.append(QVariant::fromValue(entry));
  }
  return result;
}

void SettingsForm::SetServiceSettings(const QVariantList &settings) {
  std::vector<ServiceSetting> result{};
  for(const auto &varSetting : settings) {
    auto setting = varSetting.value<ServiceSettingModel>();
    result.push_back({setting.id.toStdString(), setting.name.toStdString(), setting.enabled});
  }
  m_EditServiceSettings = result;
}

QString SettingsForm::GetOperatingSystem() {
  return QString::fromUtf8(AppInfo::GetOperatingSystem());
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
