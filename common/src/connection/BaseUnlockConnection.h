#ifndef PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
#define PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H

#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <utility>

#include "BaseConnection.h"
#include "handler/UnlockState.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/CryptUtils.h"
#include "utils/Utils.h"

struct UnlockResponseData {
  std::string unlockToken;
  std::string password;
};

class BaseUnlockConnection : public BaseConnection {
public:
  BaseUnlockConnection();
  explicit BaseUnlockConnection(const PairedDevice &device);
  virtual ~BaseUnlockConnection();

  virtual bool Start() = 0;
  virtual void Stop() = 0;

  PairedDevice GetDevice();
  UnlockResponseData GetResponseData();
  [[nodiscard]] bool HasClient() const;

  void SetUnlockInfo(const std::string &authUser, const std::string &authProgram);
  UnlockState PollResult();

protected:
  virtual void PerformAuthFlow(SOCKET socket);

private:
  std::string GetUnlockInfoPacket();
  void OnResponseReceived(const Packet &packet);

protected:
  std::atomic<bool> m_IsRunning{};
  std::thread m_AcceptThread{};
  std::atomic<bool> m_HasConnection{};
  std::string m_UserName{};

  std::atomic<UnlockState> m_UnlockState{};
  PairedDevice m_PairedDevice{};
  UnlockResponseData m_ResponseData{};

  std::string m_AuthUser{};
  std::string m_AuthProgram{};
  std::string m_UnlockToken{};
};

#endif // PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
