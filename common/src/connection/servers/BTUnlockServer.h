#ifndef PCBU_DESKTOP_BTUNLOCKSERVER_H
#define PCBU_DESKTOP_BTUNLOCKSERVER_H

#include "connection/BaseUnlockConnection.h"

class BTUnlockServer : public BaseUnlockConnection {
public:
    explicit BTUnlockServer(const PairedDevice &device);

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

#endif //PCBU_DESKTOP_BTUNLOCKSERVER_H
