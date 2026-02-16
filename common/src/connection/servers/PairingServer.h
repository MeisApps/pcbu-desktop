#ifndef PCBU_DESKTOP_PAIRINGSERVER_H
#define PCBU_DESKTOP_PAIRINGSERVER_H

#include "nlohmann/json.hpp"
#include <boost/asio.hpp>
#include <vector>

#include "PairingStructs.h"
#include "connection/BaseConnection.h"
#include "connection/Packets.h"

namespace boostnet = boost::asio::ip;

class PairingServer : public BaseConnection {
public:
  PairingServer();
  ~PairingServer() override;

  void Start(const PairingUIData &uiData);
  void Stop();

private:
  void Accept();

  std::vector<uint8_t> ReadEncryptedPacket();
  void WriteEncryptedPacket(uint8_t packetId, const std::string &data);

  boost::asio::io_context m_IOService{};
  boostnet::tcp::acceptor m_Acceptor;
  boostnet::tcp::socket m_Socket;

  PairingUIData m_UIData{};
  std::thread m_AcceptThread{};
  std::atomic<bool> m_IsRunning{};
};

#endif // PCBU_DESKTOP_PAIRINGSERVER_H
