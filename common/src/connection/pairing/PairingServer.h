#ifndef PCBU_DESKTOP_PAIRINGSERVER_H
#define PCBU_DESKTOP_PAIRINGSERVER_H

#include <nlohmann/json.hpp>
#include <vector>

#include "PairingStructs.h"
#include "connection/BaseConnection.h"
#include "connection/Packets.h"
#include "connection/SocketDefs.h"
#include "utils/CryptUtils.h"

class PairingServer : public BaseConnection {
public:
  explicit PairingServer(const std::function<void(const std::string&)>& errorCallback);
  ~PairingServer() override;

  bool Start(const PairingUIData &uiData);
  void Stop();

private:
  void AcceptThread();
  void ClientThread(SOCKET clientSocket);

  CryptPacket ReadEncryptedPacket(SOCKET clientSocket) const;
  bool WriteEncryptedPacket(SOCKET clientSocket, uint8_t packetId, const std::string &data) const;

  SOCKET m_ServerSocket = SOCKET_INVALID;
  std::thread m_AcceptThread{};
  std::atomic<bool> m_IsRunning{};

  PairingUIData m_UIData{};
  std::function<void(const std::string&)> m_ErrorCallback{};
};

#endif // PCBU_DESKTOP_PAIRINGSERVER_H
