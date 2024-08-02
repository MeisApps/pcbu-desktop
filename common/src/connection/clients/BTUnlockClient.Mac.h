#ifndef PCBU_DESKTOP_BTUNLOCKCLIENT_MAC_H
#define PCBU_DESKTOP_BTUNLOCKCLIENT_MAC_H

#include "connection/BaseUnlockConnection.h"

class BTUnlockClient : public BaseUnlockConnection {
public:
    BTUnlockClient(const std::string& deviceAddress, const PairedDevice& device);

    bool Start() override;
    void Stop() override;

private:
    void *m_Wrapper{};
};

#endif //PCBU_DESKTOP_BTUNLOCKCLIENT_MAC_H
