#ifndef PCBU_DESKTOP_PAIRINGSESSION_H
#define PCBU_DESKTOP_PAIRINGSESSION_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "connection/pairing/CloudPairingServer.h"
#include "connection/pairing/PairingStructs.h"
#include "connection/pairing/TCPPairingServer.h"
#include "connection/pairing/UDPPairingBroadcaster.h"
#include "connection/web/HttpClient.h"

struct PairingCredentialCheck {
  std::string error{};
  bool canSkip{};
};

class PairingSession {
public:
  ~PairingSession();

  void Start(const PairingUIData &data, const std::function<void()> &successCallback, const std::function<void(const std::string &)> &errorCallback);
  void Stop();

  std::string GetPayload();
  std::string GetPairingCode();

  static PairingCredentialCheck CheckCredentials(const std::string &userName, const std::string &password, bool isManualUserName,
                                                 bool skipPasswordCheck);

private:
  void StopCloudPolling();
  void CloudPollThread(const std::function<void(const std::string &)> &errorCallback);

  static constexpr int CloudPairingTimeoutSecs = 300;
  static constexpr auto CloudPollInterval = std::chrono::seconds(1);

  PairingUIData m_Data{};
  std::string m_ServerId{};
  std::string m_PollSecret{};

  std::atomic<bool> m_IsCloudPollRunning{};
  std::thread m_CloudPollThread{};
  std::unique_ptr<HttpClient> m_CloudPollClient{};

  std::unique_ptr<TCPPairingServer> m_PairingServer = nullptr;
  std::unique_ptr<CloudPairingServer> m_CloudPairingServer = nullptr;
  std::unique_ptr<UDPPairingBroadcaster> m_DiscoveryBeacon = nullptr;
};

#endif // PCBU_DESKTOP_PAIRINGSESSION_H
