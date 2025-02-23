#ifndef PCBU_DESKTOP_BTUNLOCKSERVER_MAC_H
#define PCBU_DESKTOP_BTUNLOCKSERVER_MAC_H

#include "connection/BaseUnlockConnection.h"

class BTUnlockServer : public BaseUnlockConnection {
public:
  explicit BTUnlockServer(const PairedDevice &device);

  bool Start() override;
  void Stop() override;

protected:
  void PerformAuthFlow(SOCKET socket) override;

private:
};

#endif // PCBU_DESKTOP_BTUNLOCKSERVER_MAC_H
