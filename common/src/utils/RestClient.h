#ifndef PCBU_DESKTOP_RESTCLIENT_H
#define PCBU_DESKTOP_RESTCLIENT_H

#include <string>

class HttpClient;

struct CloudRelayInfo {
  std::string sessionId{};
  std::string relayUrl{};
  std::string joinToken{};
};

enum class CloudUnlockStatus {
  Ok,
  RateLimited,
  InvalidToken,
  DeviceUnknown,
  PairingRevoked,
  NoSubscription,
  SubscriptionStale,
  DeviceStale,
  PhoneUnreachable,
  ServerUnreachable,
  UnsupportedVersion,
  Error,
};

struct CloudUnlockResult {
  CloudUnlockStatus status{CloudUnlockStatus::Error};
  CloudRelayInfo info{};
};

enum class CloudPairingStatus {
  Ok,
  Pending,
  ServerUnreachable,
  UnsupportedVersion,
  Error,
};

struct CloudPairingPollResult {
  CloudPairingStatus status{CloudPairingStatus::Error};
  CloudRelayInfo info{};
};

class RestClient {
public:
  static std::string CheckForUpdates(const std::string &product, const std::string &platform);

  static CloudPairingPollResult PollCloudPairing(const std::string &pollSecret, HttpClient *client = nullptr);
  static CloudUnlockResult RequestCloudUnlock(const std::string &cloudToken, HttpClient *client = nullptr);

private:
  RestClient() = default;
};

#endif // PCBU_DESKTOP_RESTCLIENT_H
