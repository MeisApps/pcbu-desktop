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
  explicit PairingServer(const std::function<void(const std::string &)> &errorCallback);
  ~PairingServer() override = default;

  virtual bool Start(const PairingUIData &uiData);
  virtual void Stop() = 0;

protected:
  void HandleClient(ConnectionStream &stream);

  void ReportError(const std::string &message);

  CryptPacket ReadEncryptedPacket(ConnectionStream &stream) const;
  bool WriteEncryptedPacket(ConnectionStream &stream, uint16_t packetId, const std::string &data) const;

  std::atomic<bool> m_IsRunning{};
  std::atomic<int> m_NumConnections{};

  PairingUIData m_UIData{};
  std::function<void(const std::string &)> m_ErrorCallback{};
};

#endif // PCBU_DESKTOP_PAIRINGSERVER_H
