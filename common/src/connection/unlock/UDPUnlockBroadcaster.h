#ifndef PCBU_DESKTOP_UDPUNLOCKBROADCASTER_H
#define PCBU_DESKTOP_UDPUNLOCKBROADCASTER_H

#include <cstdint>
#include <string>
#include <vector>

#include "connection/UDPBroadcaster.h"

struct UDPBroadcastDevice {
  std::string deviceID;
  uint16_t devicePort;
  bool isManual;
};

class UDPUnlockBroadcaster : public UDPBroadcaster {
public:
  UDPUnlockBroadcaster();
  ~UDPUnlockBroadcaster() override;

  void AddDevice(const std::string &deviceID, uint16_t devicePort, bool isManual);

protected:
  void SendToTarget(const BroadcastTarget &target) override;

private:
  uint16_t m_UnlockPort;
  std::vector<UDPBroadcastDevice> m_Devices{};
};

#endif // PCBU_DESKTOP_UDPUNLOCKBROADCASTER_H
