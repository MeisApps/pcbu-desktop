#ifndef PCBU_DESKTOP_UDPPAIRINGBROADCASTER_H
#define PCBU_DESKTOP_UDPPAIRINGBROADCASTER_H

#include <cstdint>
#include <string>

#include "connection/UDPBroadcaster.h"

class UDPPairingBroadcaster : public UDPBroadcaster {
public:
  UDPPairingBroadcaster(std::string serverId, const std::string &encKey);
  ~UDPPairingBroadcaster() override;

protected:
  void SendToTarget(const BroadcastTarget &target) override;

private:
  std::string m_ServerId;
  std::string m_EncKey;
  uint16_t m_PairingPort;
  uint16_t m_DiscoveryPort;
};

#endif // PCBU_DESKTOP_UDPPAIRINGBROADCASTER_H
