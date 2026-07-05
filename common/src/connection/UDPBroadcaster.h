#ifndef PCBU_DESKTOP_UDPBROADCASTER_H
#define PCBU_DESKTOP_UDPBROADCASTER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "platform/NetworkHelper.h"

class UDPBroadcaster {
public:
  virtual ~UDPBroadcaster();

  void Start();
  void Stop();

protected:
  UDPBroadcaster(std::string name, int intervalMs);

  virtual void SendToTarget(const BroadcastTarget &target) = 0;
  void SendBroadcast(const BroadcastTarget &target, const std::vector<uint8_t> &payload, uint16_t destPort);

private:
  void Run();

  std::string m_Name;
  int m_IntervalMs;
  std::thread m_Thread{};
  std::atomic<bool> m_IsRunning{};
};

#endif // PCBU_DESKTOP_UDPBROADCASTER_H
