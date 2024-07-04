#ifndef PAM_PCBIOUNLOCK_BTUNLOCKCLIENT_H
#define PAM_PCBIOUNLOCK_BTUNLOCKCLIENT_H

#include "connection/BaseUnlockServer.h"

class BTUnlockClient : public BaseUnlockServer {
public:
    BTUnlockClient(const std::string& deviceAddress, const PairedDevice& device);

    bool Start() override;
    void Stop() override;

private:
    void ConnectThread();

    int m_Channel;
    SOCKET m_ClientSocket;
    std::string m_DeviceAddress;
};

#endif //PAM_PCBIOUNLOCK_BTUNLOCKCLIENT_H
