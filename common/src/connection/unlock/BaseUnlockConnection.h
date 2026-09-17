#ifndef PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
#define PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H

#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
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
  UnlockState PollResult();

protected:
  void SetPhase(UnlockPhase phase);
  void PerformAuthFlow(ConnectionStream &stream, bool needsDeviceID = false);

private:
  void OnPacketReceived(ConnectionStream &stream, Packet &packet);
  bool SendUnlockRequest(ConnectionStream &stream);
  void OnResponseReceived(const Packet &packet);

protected:
  std::atomic<bool> m_IsRunning{};
  std::thread m_AcceptThread{};
  std::atomic<UnlockPhase> m_Phase{UnlockPhase::STARTING};
  std::string m_UserName{};

  std::atomic<UnlockState> m_UnlockState{};
  PairedDevice m_PairedDevice{};
  PacketUnlockResponseData m_ResponseData{};

  std::map<ConnectionStream *, UnlockConnectionState> m_ConnectionStates{};
  std::mutex m_StateMutex{};

  std::string m_AuthUser{};
  std::string m_AuthProgram{};
  std::string m_UnlockToken{};
};

#endif // PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
