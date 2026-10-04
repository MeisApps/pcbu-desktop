#ifndef PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
#define PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H

#include <atomic>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <thread>
#include <utility>

#include "../BaseConnection.h"
#include "../Packets.h"
#include "handler/UnlockState.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/CryptUtils.h"
#include "utils/Utils.h"

enum UnlockConnectionState {
  NONE,
  HAS_DEVICE_ID,
  HAS_UNLOCK_REQUEST,
  HAS_UNLOCK_RESPONSE,
};

class BaseUnlockConnection : public BaseConnection {
public:
  BaseUnlockConnection();
  explicit BaseUnlockConnection(const PairedDevice &device);
  virtual ~BaseUnlockConnection();

  virtual bool Start() = 0;
  virtual void Stop() = 0;

  PairedDevice GetDevice();
  PacketUnlockResponseData GetResponseData();
  [[nodiscard]] UnlockPhase GetPhase() const;

  void SetUnlockInfo(const std::string &authUser, const std::string &authProgram);
  void SetAllowedDevices(const std::vector<PairedDevice> &devices);

  UnlockState PollResult();

protected:
  void SetPhase(UnlockPhase phase);
  void PerformAuthFlow(ConnectionStream &stream, bool needsDeviceID = false);

private:
  struct ConnectionInfo {
    UnlockConnectionState state{};
    PairedDevice device{};
  };

  bool OnPacketReceived(ConnectionStream &stream, const Packet &packet, bool isServerConnection);
  bool OnResponseReceived(ConnectionStream &stream, const Packet &packet, const PairedDevice &device, bool isServerConnection);
  PacketError SendUnlockRequest(ConnectionStream &stream, const PairedDevice &device);

  bool HandleUnverifiedError(UnlockState state, bool isServerConnection);
  std::optional<PairedDevice> FindAllowedDevice(const std::string &deviceId);
  static UnlockState MapPacketError(PacketError error, UnlockState fallback);

protected:
  std::atomic<bool> m_IsRunning{};
  std::thread m_AcceptThread{};
  std::atomic<UnlockPhase> m_Phase{UnlockPhase::STARTING};
  std::string m_UserName{};

  std::atomic<UnlockState> m_UnlockState{};
  PairedDevice m_PairedDevice{};
  PacketUnlockResponseData m_ResponseData{};

  std::map<ConnectionStream *, ConnectionInfo> m_Connections{};
  std::vector<PairedDevice> m_AllowedDevices{};
  std::mutex m_StateMutex{};

  std::string m_AuthUser{};
  std::string m_AuthProgram{};
  std::string m_UnlockToken{};
};

#endif // PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
