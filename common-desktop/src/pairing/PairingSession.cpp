#include "PairingSession.h"

#include "platform/NetworkHelper.h"
#include "platform/PlatformHelper.h"
#include "storage/AppSettings.h"
#include "utils/CryptUtils.h"
#include "utils/I18n.h"
#include "utils/RestClient.h"
#include "utils/StringUtils.h"

PairingSession::~PairingSession() {
  Stop();
}

void PairingSession::Start(const PairingUIData &data, const std::function<void()> &successCallback,
                           const std::function<void(const std::string &)> &errorCallback) {
  Stop();

  auto isCloud = data.method == PairingMethod::CLOUD;
  if(isCloud)
    m_CloudPairingServer = std::make_unique<CloudPairingServer>(successCallback, errorCallback);
  else
    m_PairingServer = std::make_unique<TCPPairingServer>(successCallback, errorCallback);
  m_Data = data;
  m_ServerId = StringUtils::RandomString(8);
  m_Data.encKey = StringUtils::RandomString(64);
  if(isCloud) {
    m_PollSecret = StringUtils::RandomString(48);
    m_ServerId = CryptUtils::Sha256(m_PollSecret);
  }
  m_Data.macAddress = NetworkHelper::GetSavedNetworkInterface().macAddress;
  if(isCloud) {
    m_CloudPairingServer->Start(m_Data);
    m_CloudPollClient = std::make_unique<HttpClient>();
    m_IsCloudPollRunning = true;
    m_CloudPollThread = std::thread(&PairingSession::CloudPollThread, this, errorCallback);
  } else {
    m_PairingServer->Start(m_Data);
  }
  if(!m_Data.useLegacy && !isCloud) {
    m_DiscoveryBeacon = std::make_unique<UDPPairingBroadcaster>(m_ServerId, m_Data.encKey);
    m_DiscoveryBeacon->Start();
  }
}

void PairingSession::Stop() {
  StopCloudPolling();
  m_ServerId = {};
  m_PollSecret = {};
  m_Data.encKey = {};
  if(m_PairingServer) {
    m_PairingServer->Stop();
  }
  if(m_CloudPairingServer) {
    m_CloudPairingServer->Stop();
  }
  m_PairingServer.reset();
  m_CloudPairingServer.reset();
  if(m_DiscoveryBeacon) {
    m_DiscoveryBeacon->Stop();
    m_DiscoveryBeacon.reset();
  }
}

std::string PairingSession::GetPayload() {
  auto settings = AppSettings::Get();
  if(m_Data.useLegacy && m_Data.method != PairingMethod::CLOUD) {
    return LegacyPairingQRData(NetworkHelper::GetSavedNetworkInterface().ipAddress, settings.pairingServerPort, m_Data.method, m_Data.encKey)
        .ToJson()
        .dump();
  }
  return PairingQRData(m_ServerId, settings.pairingDiscoveryPort, m_Data.method, m_Data.encKey).ToJson().dump();
}

std::string PairingSession::GetPairingCode() {
  return StringUtils::WithSeperators(StringUtils::ToBase32String(GetPayload()), "-", 5);
}

PairingCredentialCheck PairingSession::CheckCredentials(const std::string &userName, const std::string &password, bool isManualUserName,
                                                        bool skipPasswordCheck) {
#ifdef WINDOWS
  if(StringUtils::Split(userName, "\\").size() != 2 && userName.find('@') == std::string::npos)
    return {I18n::Get("error_win_invalid_user")};
#endif
  if(skipPasswordCheck)
    return {};

  if(!PlatformHelper::HasUserPassword(userName))
    return {isManualUserName ? I18n::Get("error_invalid_user_or_no_password") : I18n::Get("error_user_no_password")};
  auto loginStatus = PlatformHelper::CheckLogin(userName, password);
  switch(loginStatus.result) {
    case PlatformLoginResult::INVALID_USER:
      return {I18n::Get("error_invalid_user_input")};
    case PlatformLoginResult::INVALID_PASSWORD:
      return {isManualUserName ? I18n::Get("error_invalid_user_or_incorrect_password") : I18n::Get("error_incorrect_password"), true};
    case PlatformLoginResult::ACCOUNT_LOCKED:
      return {I18n::Get("error_win_account_locked")};
    case PlatformLoginResult::ACCOUNT_RESTRICTED:
      return {I18n::Get("error_win_account_restricted")};
    case PlatformLoginResult::PW_EXPIRED:
      return {I18n::Get("error_win_password_expired")};
    case PlatformLoginResult::ACCOUNT_DISABLED:
      return {I18n::Get("error_account_disabled")};
    case PlatformLoginResult::LOGON_TYPE_DENIED:
      return {I18n::Get("error_win_logon_type_denied")};
    case PlatformLoginResult::UNKNOWN_ERROR: {
      auto errorStr = I18n::Get("error_login_unknown");
      if(loginStatus.errorCode != 0)
        errorStr = I18n::Get("error_with_code", errorStr, loginStatus.errorCode);
      return {errorStr};
    }
    case PlatformLoginResult::SUCCESS:
      break;
  }
  return {};
}

void PairingSession::StopCloudPolling() {
  m_IsCloudPollRunning = false;
  if(m_CloudPollClient)
    m_CloudPollClient->Cancel();
  if(m_CloudPollThread.joinable())
    m_CloudPollThread.join();
  m_CloudPollClient.reset();
}

void PairingSession::CloudPollThread(const std::function<void(const std::string &)> &errorCallback) {
  auto pollSecret = m_PollSecret;
  auto startTime = std::chrono::steady_clock::now();
  auto lastPollTime = startTime - CloudPollInterval;
  auto failures = 0;
  while(m_IsCloudPollRunning) {
    if(std::chrono::steady_clock::now() - startTime > std::chrono::seconds(CloudPairingTimeoutSecs)) {
      errorCallback(I18n::Get("error_cloud_pairing_expired"));
      return;
    }
    if(std::chrono::steady_clock::now() - lastPollTime >= CloudPollInterval) {
      lastPollTime = std::chrono::steady_clock::now();
      auto result = RestClient::PollCloudPairing(pollSecret, m_CloudPollClient.get());
      if(result.status == CloudPairingStatus::Ok) {
        if(m_CloudPairingServer)
          m_CloudPairingServer->ConnectRelay(result.info.relayUrl, result.info.sessionId, result.info.joinToken);
        return;
      }
      if(result.status == CloudPairingStatus::Pending) {
        failures = 0;
      } else if(result.status != CloudPairingStatus::ServerUnreachable || ++failures >= 5) {
        if(m_IsCloudPollRunning) {
          auto key = "error_cloud_pairing";
          if(result.status == CloudPairingStatus::ServerUnreachable)
            key = "error_cloud_unreachable";
          else if(result.status == CloudPairingStatus::UnsupportedVersion)
            key = "error_protocol_mismatch";
          errorCallback(I18n::Get(key));
        }
        return;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}
