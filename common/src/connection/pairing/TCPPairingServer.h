#ifndef PCBU_DESKTOP_TCPPAIRINGSERVER_H
#define PCBU_DESKTOP_TCPPAIRINGSERVER_H

#include "PairingServer.h"

class TCPPairingServer : public PairingServer {
public:
  using PairingServer::PairingServer;
  ~TCPPairingServer() override;

  bool Start(const PairingUIData &uiData) override;
  void Stop() override;

private:
  void AcceptThread();
  void ClientThread(SOCKET clientSocket);

  SOCKET m_ServerSocket = SOCKET_INVALID;
  std::thread m_AcceptThread{};
};

#endif // PCBU_DESKTOP_TCPPAIRINGSERVER_H
