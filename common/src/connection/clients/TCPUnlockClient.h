#ifndef PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H
#define PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H

#include "connection/BaseUnlockServer.h"

#ifdef _WIN32
typedef unsigned long long SOCKET;
#else
#define SOCKET int
#endif

class TCPUnlockClient : public BaseUnlockServer {
public:
    TCPUnlockClient(const std::string& ipAddress, int port, const PairedDevice& device);

    bool Start() override;
    void Stop() override;

private:
    std::vector<uint8_t> ReadPacket() const;
    void WritePacket(const std::vector<uint8_t>& data) const;

    void ConnectThread();

    std::string m_IP;
    int m_Port;
    SOCKET m_ClientSocket;
    std::string m_DeviceAddress;
};

#endif //PAM_PCBIOUNLOCK_TCPUNLOCKCLIENT_H
