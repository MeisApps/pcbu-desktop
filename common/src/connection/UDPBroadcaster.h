#ifndef UDPBROADCASTER_H
#define UDPBROADCASTER_H

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

class UDPBroadcaster {
public:
  ~UDPBroadcaster();

  void Start();
  void Stop();

  void AddDevice(const std::string &deviceID, uint16_t devicePort);

private:
  std::thread m_Thread{};
  std::atomic<bool> m_IsRunning{};
  std::vector<std::pair<std::string, uint16_t>> m_Devices{};
};

#endif
