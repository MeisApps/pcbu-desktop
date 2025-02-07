#include "I18n.h"

#include <map>

#include "utils/LocaleHelper.h"

const std::map<std::string, std::string> g_EnLangMap = {
        {"initializing", "Initializing..."},
        {"password", "Password"},
        {"error_unknown", "Unknown Error."},
        {"error_pam", "Error: Could not get PAM info."},
        {"error_not_paired", "Error: User {} is not paired."},
        {"error_invalid_user", "Error: Invalid user."},
        {"error_start_handler", "Error: Could not start socket."},
        {"error_password", "Invalid password."},
        {"error_protocol_mismatch", "Your app's version is incompatible with the desktop app's version. Please make sure that both are up-to-date."},
        {"error_pairing_packet_parse", "Failed to parse pairing data. Please make sure that both the desktop and mobile app are up-to-date."},
        {"enter_password", "Please enter your password."},
        {"wait_network", "Waiting for network connection..."},
        {"wait_key_press", "Press any key or click."},
        {"wait_client_phone_connect", "Connecting to phone..."},
        {"wait_server_phone_connect", "Waiting for connection from phone..."},
        {"wait_phone_unlock", "Use phone to unlock..."},
        {"unlock_success", "Success."},
        {"unlock_canceled", "Canceled."},
        {"unlock_timeout", "Timeout."},
        {"unlock_error_connect", "Could not connect to phone."},
        {"unlock_error_time", "Error: Time on PC does not match phone time."},
        {"unlock_error_data", "Error: Invalid data received."},
        {"unlock_error_not_paired", "Error: Not paired on phone."},
        {"unlock_error_app", "Unknown app error. Please contact support."},
        {"unlock_error_unknown", "Unknown error. Please contact support."}
};
const std::map<std::string, std::string> g_DeLangMap = {
        {"initializing", "Initialisiere..."},
        {"password", "Passwort"},
        {"error_unknown", "Unbekannter Fehler."},
        {"error_pam", "Fehler: Konnte PAM Infos nicht holen."},
        {"error_not_paired", "Fehler: Benutzer {} ist nicht gepairt."},
        {"error_invalid_user", "Fehler: Ungültiger Benutzer."},
        {"error_start_handler", "Fehler: Konnte Socket nicht starten."},
        {"error_password", "Ungültiges Passwort."},
        {"error_protocol_mismatch", "Die Version deiner App ist inkompatibel mit der Desktop Version. Stelle bitte sicher, dass beide aktuell sind."},
        {"error_pairing_packet_parse", "Kopplungsdaten konnten nicht verarbeitet werden. Stelle bitte sicher, dass die Desktop- und Smartphone-App aktuell ist."},
        {"enter_password", "Gib bitte dein Passwort ein."},
        {"wait_network", "Warte auf Netzwerkverbindung..."},
        {"wait_key_press", "Drücke eine beliebige Taste."},
        {"wait_client_phone_connect", "Verbinde mit Telefon..."},
        {"wait_server_phone_connect", "Warte auf Verbindung von Telefon..."},
        {"wait_phone_unlock", "Verwende Telefon, um zu entsperren..."},
        {"unlock_success", "Erfolg."},
        {"unlock_canceled", "Abgebrochen."},
        {"unlock_timeout", "Zeit abgelaufen."},
        {"unlock_error_connect", "Verbindung zum Telefon fehlgeschlagen."},
        {"unlock_error_time", "Fehler: Zeit auf PC stimmt nicht mit Telefon Zeit überein."},
        {"unlock_error_data", "Fehler: Ungültige Daten empfangen."},
        {"unlock_error_not_paired", "Fehler: Nicht gepairt auf Telefon."},
        {"unlock_error_app", "Unbekannter App Fehler. Bitte Support kontaktieren."},
        {"unlock_error_unknown", "Unbekannter Fehler. Bitte Support kontaktieren."}
};
const std::map<std::string, std::string> g_ZhLangMap = {
        {"initializing", "初始化中..."},
        {"password", "密码"},
        {"error_unknown", "未知错误。"},
        {"error_pam", "错误：无法获取 PAM 信息。"},
        {"error_not_paired", "错误：用户 {} 未配对。"},
        {"error_invalid_user", "错误：无效用户。"},
        {"error_start_handler", "错误：无法启动套接字。"},
        {"error_password", "密码错误。"},
        {"error_protocol_mismatch", "您的应用程序版本与桌面应用程序版本不兼容。请确保两者都是最新版本。"},
        {"error_pairing_packet_parse", "无法解析配对数据。请确保桌面和移动应用程序都是最新版本。"},
        {"enter_password", "请输入密码。"},
        {"wait_network", "等待网络连接..."},
        {"wait_key_press", "按任意键或点击。"},
        {"wait_client_phone_connect", "正在连接到手机..."},
        {"wait_server_phone_connect", "等待手机连接..."},
        {"wait_phone_unlock", "使用手机解锁..."},
        {"unlock_success", "成功。"},
        {"unlock_canceled", "已取消。"},
        {"unlock_timeout", "超时。"},
        {"unlock_error_connect", "无法连接到手机。"},
        {"unlock_error_time", "错误：电脑时间与手机时间不匹配。"},
        {"unlock_error_data", "错误：接收到无效数据。"},
        {"unlock_error_not_paired", "错误：手机未配对。"},
        {"unlock_error_app", "未知的应用错误，请联系支持。"},
        {"unlock_error_unknown", "未知错误，请联系支持。"}
};

std::string I18n::Get(const std::string &key) {
    std::map<std::string, std::string> langMap{};
    switch (LocaleHelper::GetUserLocale()) {
        case LocaleHelper::Locale::GERMAN:
            langMap = g_DeLangMap;
            break;
        case LocaleHelper::Locale::CHINESE_SIMPLIFIED:
            langMap = g_ZhLangMap;
            break;
        case LocaleHelper::Locale::ENGLISH:
        default:
            langMap = g_EnLangMap;
            break;
    }
    if(!langMap.count(key)) {
        spdlog::warn("Missing I18n key {}.", key);
        return key;
    }
    return langMap[key];
}
