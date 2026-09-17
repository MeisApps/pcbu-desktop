#include "CloudUnlockClient.h"

#include <spdlog/spdlog.h>

#include "utils/RestClient.h"

CloudUnlockClient::CloudUnlockClient(const PairedDevice &device) : BaseUnlockConnection(device) {}

CloudUnlockClient::~CloudUnlockClient() {
  Stop();
}

bool CloudUnlockClient::Start() {
  if(m_IsRunning)
    return true;

  m_IsRunning = true;
  SetPhase(UnlockPhase::CLOUD_REQUESTING);
  m_HttpClient = std::make_unique<HttpClient>();
  m_Stream = std::make_unique<WebSocketStream>();
  m_AcceptThread = std::thread(&CloudUnlockClient::ConnectThread, this);
  return true;
}

void CloudUnlockClient::Stop() {
  m_IsRunning = false;
  if(m_HttpClient)
    m_HttpClient->Cancel();
  if(m_Stream)
    m_Stream->Close();
  if(m_AcceptThread.joinable())
    m_AcceptThread.join();
}

void CloudUnlockClient::ConnectThread() {
  spdlog::info("Connecting via cloud...");
  auto result = RestClient::RequestCloudUnlock(m_PairedDevice.cloudToken, m_HttpClient.get());
  if(result.status != CloudUnlockStatus::Ok) {
    if(!m_IsRunning)
      return;
    m_UnlockState = MapRequestError(result.status);
    m_IsRunning = false;
    return;
  }

  SetPhase(UnlockPhase::CLOUD_CONNECTING);
  if(!m_Stream->Connect(result.info.relayUrl, result.info.sessionId, result.info.joinToken, "pc")) {
    m_UnlockState = UnlockState::CONNECT_ERROR;
    m_IsRunning = false;
    return;
  }

  PerformAuthFlow(*m_Stream);
  m_IsRunning = false;
}

UnlockState CloudUnlockClient::MapRequestError(CloudUnlockStatus status) {
  switch(status) {
    case CloudUnlockStatus::RateLimited:
      return UnlockState::CLOUD_RATE_LIMITED;
    case CloudUnlockStatus::InvalidToken:
      return UnlockState::NOT_PAIRED_ERROR;
    case CloudUnlockStatus::DeviceUnknown:
    case CloudUnlockStatus::PairingRevoked:
      return UnlockState::CLOUD_PAIRING_GONE;
    case CloudUnlockStatus::SubscriptionStale:
      return UnlockState::CLOUD_SUBSCRIPTION_STALE;
    case CloudUnlockStatus::DeviceStale:
      return UnlockState::CLOUD_DEVICE_STALE;
    case CloudUnlockStatus::PhoneUnreachable:
      return UnlockState::CLOUD_PHONE_UNREACHABLE;
    case CloudUnlockStatus::ServerUnreachable:
      return UnlockState::CLOUD_SERVER_UNREACHABLE;
    case CloudUnlockStatus::NoSubscription:
      return UnlockState::CLOUD_NO_SUBSCRIPTION;
    case CloudUnlockStatus::UnsupportedVersion:
      return UnlockState::PROTOCOL_ERROR;
    default:
      return UnlockState::UNK_ERROR;
  }
}
