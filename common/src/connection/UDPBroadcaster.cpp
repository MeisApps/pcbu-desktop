#include "UDPBroadcaster.h"

#include "Packets.h"
#include "platform/NetworkHelper.h"
#include "storage/AppSettings.h"

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

constexpr int INTERVAL_SECS = 2;

UDPBroadcaster::~UDPBroadcaster() {
  Stop();
}

void UDPBroadcaster::Start() {
  if(m_IsRunning.exchange(true))
    return;
  m_Thread = std::thread([this]() {
    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket(io_context);
    socket.open(boost::asio::ip::udp::v4());
    socket.set_option(boost::asio::socket_base::broadcast(true));

    auto dataVec = std::vector<std::pair<boost::asio::ip::udp::endpoint, nlohmann::json>>();
    for(const auto &device : m_Devices) {
      boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::broadcast(), device.devicePort);
      auto packet = PacketUDPBroadcast();
      packet.deviceId = device.deviceID;
      packet.pcbuIP = NetworkHelper::GetSavedNetworkInterface().ipAddress;
      packet.pcbuPort = AppSettings::Get().unlockServerPort;
      packet.isManual = device.isManual;
      dataVec.emplace_back(endpoint, packet.ToJson());
    }

    spdlog::info("UDP broadcast started.");
    auto startTime = std::chrono::high_resolution_clock::now();
    while(m_IsRunning.load()) {
      auto diff = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - startTime).count();
      if(diff >= INTERVAL_SECS) {
        for(const auto &data : dataVec)
          socket.send_to(boost::asio::buffer(data.second.dump()), data.first);
        startTime = std::chrono::high_resolution_clock::now();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    spdlog::info("UDP broadcast stopped.");
  });
}

void UDPBroadcaster::Stop() {
  if(!m_IsRunning.load())
    return;
  m_IsRunning.store(false);
  if(m_Thread.joinable())
    m_Thread.join();
}

void UDPBroadcaster::AddDevice(const std::string &deviceID, uint16_t devicePort, bool isManual) {
  m_Devices.emplace_back(deviceID, devicePort, isManual);
}
