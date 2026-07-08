#ifndef PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H
#define PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H

#include "connection/unlock/BaseUnlockConnection.h"

class TCPUnlockClient : public BaseUnlockConnection {
public:
  TCPUnlockClient(const PairedDevice &device);

  bool Start() override;
  void Stop() override;

private:
  bool ConnectToAddress(const std::string &ipAddress, uint32_t timeoutSeconds);
  void ConnectThread();

  int m_Port;
  SOCKET m_ClientSocket;
};

#endif // PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H
