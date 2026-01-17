#ifndef UDPBROADCASTER_H
#define UDPBROADCASTER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

struct UDPBroadcastDevice {
  std::string deviceID;
  uint16_t devicePort;
  bool isManual;
};

class UDPBroadcaster {
public:
  ~UDPBroadcaster();

  void Start();
  void Stop();

  void AddDevice(const std::string &deviceID, uint16_t devicePort, bool isManual);

private:
  std::thread m_Thread{};
  std::atomic<bool> m_IsRunning{};
  std::vector<UDPBroadcastDevice> m_Devices{};
};

#endif
