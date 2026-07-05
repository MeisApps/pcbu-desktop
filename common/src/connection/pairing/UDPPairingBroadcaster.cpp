#include "UDPPairingBroadcaster.h"

#include "connection/Packets.h"
#include "storage/AppSettings.h"
#include "utils/CryptUtils.h"

#include <spdlog/spdlog.h>

constexpr int INTERVAL_MS = 2000;

UDPPairingBroadcaster::UDPPairingBroadcaster(std::string serverId, const std::string &encKey)
    : UDPBroadcaster("UDPPairingBroadcaster", INTERVAL_MS), m_ServerId(std::move(serverId)) {
  m_EncKey = encKey;
  m_PairingPort = AppSettings::Get().pairingServerPort;
  m_DiscoveryPort = AppSettings::Get().pairingDiscoveryPort;
}

UDPPairingBroadcaster::~UDPPairingBroadcaster() {
  Stop();
}

void UDPPairingBroadcaster::SendToTarget(const BroadcastTarget &target) {
  auto packet = PacketUDPPairBeacon();
  packet.serverId = m_ServerId;
  packet.ip = target.sourceIP;
  packet.port = m_PairingPort;

  auto plain = packet.ToJson().dump();
  auto enc = CryptUtils::EncryptAESPacket({plain.begin(), plain.end()}, m_EncKey);
  if(enc.result != PacketCryptResult::OK) {
    spdlog::warn("UDPPairingBroadcaster: Failed to encrypt beacon.");
    return;
  }
  SendBroadcast(target, enc.data, m_DiscoveryPort);
}
