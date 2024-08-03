#ifndef PCBU_DESKTOP_TCPUNLOCKSERVER_H
#define PCBU_DESKTOP_TCPUNLOCKSERVER_H

#include "connection/BaseUnlockConnection.h"

class TCPUnlockServer : public BaseUnlockConnection {
public:
    TCPUnlockServer();

    bool Start() override;
    void Stop() override;

protected:
    void PerformAuthFlow(SOCKET socket) override;

private:
    void AcceptThread();
    void ClientThread(SOCKET clientSocket);

    SOCKET m_ServerSocket;
    std::vector<SOCKET> m_ClientSockets{};
    std::vector<std::thread> m_ClientThreads{};
};

#endif //PCBU_DESKTOP_TCPUNLOCKSERVER_H
