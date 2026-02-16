#ifndef PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
#define PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H

#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <utility>

#include "BaseConnection.h"
#include "Packets.h"
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
  [[nodiscard]] bool HasClient() const;

  void SetUnlockInfo(const std::string &authUser, const std::string &authProgram);
  UnlockState PollResult();

protected:
  void PerformAuthFlow(SOCKET socket, bool needsDeviceID = false);

private:
  void OnPacketReceived(SOCKET socket, Packet &packet);
  bool SendUnlockRequest(SOCKET socket);
  void OnResponseReceived(const Packet &packet);

protected:
  std::atomic<bool> m_IsRunning{};
  std::thread m_AcceptThread{};
  std::atomic<bool> m_HasConnection{};
  std::string m_UserName{};

  std::atomic<UnlockState> m_UnlockState{};
  PairedDevice m_PairedDevice{};
  PacketUnlockResponseData m_ResponseData{};

  std::map<SOCKET, UnlockConnectionState> m_ConnectionStates{};
  std::mutex m_StateMutex{};

  std::string m_AuthUser{};
  std::string m_AuthProgram{};
  std::string m_UnlockToken{};
};

#endif // PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
