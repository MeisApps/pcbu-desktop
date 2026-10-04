#ifndef PCBU_DESKTOP_TCPUNLOCKSERVER_H
#define PCBU_DESKTOP_TCPUNLOCKSERVER_H

#include "connection/unlock/BaseUnlockConnection.h"

class TCPUnlockServer : public BaseUnlockConnection {
public:
  TCPUnlockServer();

  bool Start() override;
  void Stop() override;

private:
  void AcceptThread();
  void ClientThread(SOCKET clientSocket, uint32_t idleTimeoutSecs);

  SOCKET m_ServerSocket;
  std::atomic<int> m_NumConnections{};
};

#endif // PCBU_DESKTOP_TCPUNLOCKSERVER_H
