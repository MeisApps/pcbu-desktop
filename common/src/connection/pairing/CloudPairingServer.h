#ifndef PCBU_DESKTOP_CLOUDPAIRINGSERVER_H
#define PCBU_DESKTOP_CLOUDPAIRINGSERVER_H

#include "PairingServer.h"
#include "connection/stream/WebSocketStream.h"

class CloudPairingServer : public PairingServer {
public:
  using PairingServer::PairingServer;
  ~CloudPairingServer() override;

  bool ConnectRelay(const std::string &relayUrl, const std::string &sessionId, const std::string &joinToken);
  void Stop() override;

private:
  void RelayThread(const std::string &relayUrl, const std::string &sessionId, const std::string &joinToken);

  std::thread m_RelayThread{};
  std::unique_ptr<WebSocketStream> m_RelayStream{};
};

#endif // PCBU_DESKTOP_CLOUDPAIRINGSERVER_H
