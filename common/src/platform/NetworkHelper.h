#ifndef PCBU_DESKTOP_NETWORKHELPER_H
#define PCBU_DESKTOP_NETWORKHELPER_H

#include <string>
#include <vector>

struct NetworkInterface {
  std::string ifName{};
  std::string ipAddress{};
  std::string netmask{};
  std::string macAddress{};
  std::string gateway{};
  bool isWired{};
};

struct BroadcastTarget {
  std::string sourceIP{};
  std::string broadcastIP{};
};

class NetworkHelper {
public:
  static std::vector<NetworkInterface> GetLocalNetInterfaces(bool onlyValid = false);
  static std::string GetHostName();
  static bool HasLANConnection();

  static NetworkInterface GetSavedNetworkInterface();

  static std::vector<NetworkInterface> GetWakeOnLanInterfaces();
  static std::vector<BroadcastTarget> GetBroadcastTargets();

private:
  NetworkHelper() = default;

  static bool IsWiredEthernet(const std::string &ifName);
};

#endif // PCBU_DESKTOP_NETWORKHELPER_H
