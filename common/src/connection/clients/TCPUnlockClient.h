#ifndef PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H
#define PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H

#include "connection/BaseUnlockServer.h"

class TCPUnlockClient : public BaseUnlockServer {
public:
    TCPUnlockClient(const std::string& ipAddress, int port, const PairedDevice& device);

    bool Start() override;
    void Stop() override;

private:
    void ConnectThread();

    std::string m_IP;
    int m_Port;
    SOCKET m_ClientSocket;
};

#endif //PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H
