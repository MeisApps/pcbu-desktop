#include "UDPBroadcaster.h"

#include <chrono>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

UDPBroadcaster::UDPBroadcaster(std::string name, int intervalMs) : m_Name(std::move(name)), m_IntervalMs(intervalMs) {}

UDPBroadcaster::~UDPBroadcaster() {
  Stop();
}

void UDPBroadcaster::Start() {
  if(m_IsRunning.exchange(true))
    return;
  m_Thread = std::thread([this]() { Run(); });
}

void UDPBroadcaster::Stop() {
  if(!m_IsRunning.exchange(false))
    return;
  if(m_Thread.joinable())
    m_Thread.join();
}

void UDPBroadcaster::Run() {
  spdlog::info("{} started.", m_Name);
  auto broadcastTargets = NetworkHelper::GetBroadcastTargets();
  auto lastSend = std::chrono::steady_clock::now() - std::chrono::milliseconds(m_IntervalMs);
  while(m_IsRunning.load()) {
    auto now = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSend).count() < m_IntervalMs) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }
    lastSend = now;

    try {
      for(const auto &target : broadcastTargets)
        SendToTarget(target);
    } catch(const std::exception &ex) {
      spdlog::error("{}: Unexpected error while broadcasting: {}", m_Name, ex.what());
    }
  }
  spdlog::info("{} stopped.", m_Name);
}

void UDPBroadcaster::SendBroadcast(const BroadcastTarget &target, const std::vector<uint8_t> &payload, uint16_t destPort) {
  boost::system::error_code ec;
  boost::asio::io_context io_context;
  boost::asio::ip::udp::socket socket(io_context);

  socket.open(boost::asio::ip::udp::v4(), ec);
  if(ec) {
    spdlog::warn("{}: Failed to open socket for {}: {}", m_Name, target.sourceIP, ec.message());
    return;
  }
  socket.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if(ec)
    spdlog::warn("{}: Failed to set reuse_address for {}: {}", m_Name, target.sourceIP, ec.message());
  socket.set_option(boost::asio::socket_base::broadcast(true), ec);
  if(ec)
    spdlog::warn("{}: Failed to set broadcast for {}: {}", m_Name, target.sourceIP, ec.message());

  auto srcAddr = boost::asio::ip::make_address_v4(target.sourceIP, ec);
  if(ec) {
    spdlog::warn("{}: Invalid source address '{}': {}", m_Name, target.sourceIP, ec.message());
    return;
  }
  socket.bind(boost::asio::ip::udp::endpoint(srcAddr, 0), ec);
  if(ec) {
    spdlog::warn("{}: Failed to bind to {}: {}", m_Name, target.sourceIP, ec.message());
    return;
  }
  auto bcAddr = boost::asio::ip::make_address_v4(target.broadcastIP, ec);
  if(ec) {
    spdlog::warn("{}: Invalid broadcast address '{}': {}", m_Name, target.broadcastIP, ec.message());
    return;
  }
  socket.send_to(boost::asio::buffer(payload), boost::asio::ip::udp::endpoint(bcAddr, destPort), 0, ec);
  if(ec)
    spdlog::warn("{}: Send failed ({} -> {}:{}): {}", m_Name, target.sourceIP, target.broadcastIP, destPort, ec.message());
}
