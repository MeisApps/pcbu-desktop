#include "UDPUnlockBroadcaster.h"

#include "connection/Packets.h"
#include "storage/AppSettings.h"

constexpr int INTERVAL_MS = 2000;

UDPUnlockBroadcaster::UDPUnlockBroadcaster() : UDPBroadcaster("UDPUnlockBroadcaster", INTERVAL_MS) {
  m_UnlockPort = AppSettings::Get().unlockServerPort;
}

UDPUnlockBroadcaster::~UDPUnlockBroadcaster() {
  Stop();
}

void UDPUnlockBroadcaster::AddDevice(const std::string &deviceID, uint16_t devicePort, bool isManual) {
  m_Devices.emplace_back(deviceID, devicePort, isManual);
}

void UDPUnlockBroadcaster::SendToTarget(const BroadcastTarget &target) {
  for(const auto &device : m_Devices) {
    auto packet = PacketUDPBroadcast();
    packet.deviceId = device.deviceID;
    packet.pcbuIP = target.sourceIP;
    packet.pcbuPort = m_UnlockPort;
    packet.isManual = device.isManual;
    auto payload = packet.ToJson().dump();
    SendBroadcast(target, {payload.begin(), payload.end()}, device.devicePort);
  }
}
