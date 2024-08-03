#include "PairingServer.h"

#include "platform/PlatformHelper.h"
#include "platform/NetworkHelper.h"
#include "storage/AppSettings.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/AppInfo.h"
#include "utils/CryptUtils.h"

PairingServer::PairingServer()
    : m_Acceptor(m_IOService, boostnet::tcp::endpoint(boostnet::tcp::v4(), AppSettings::Get().pairingServerPort)),
      m_Socket(m_IOService) {

}

PairingServer::~PairingServer() {
    Stop();
}

void PairingServer::Start(const PairingServerData& serverData) {
    m_ServerData = serverData;
    if(m_IsRunning)
        return;
    m_IsRunning = true;
    m_AcceptThread = std::thread([this]() {
        spdlog::info("Pairing server started.");
        try {
            m_Acceptor.listen();
            Accept();
            m_IOService.run();
        } catch(const std::exception& ex) {
            spdlog::error("Pairing server error: {}", ex.what());
        }
        spdlog::info("Pairing server stopped.");
    });
}

void PairingServer::Stop() {
    if(!m_IsRunning)
        return;
    try {
        if(m_Socket.is_open()) {
            try {
                m_Socket.shutdown(boost::asio::socket_base::shutdown_both);
            } catch (...) {}
            m_Socket.close();
        }
        if(m_Acceptor.is_open())
            m_Acceptor.close();
        m_IOService.stop();
        if(m_AcceptThread.joinable())
            m_AcceptThread.join();
        m_IOService.reset();
        m_Acceptor = boostnet::tcp::acceptor(m_IOService, boostnet::tcp::endpoint(boostnet::tcp::v4(), AppSettings::Get().pairingServerPort));
        m_Socket = boostnet::tcp::socket(m_IOService);
    } catch(const std::exception& ex) {
        spdlog::error("Error stopping pairing server: {}", ex.what());
    }
    m_IsRunning = false;
}

void PairingServer::Accept() {
    m_Acceptor.async_accept(m_Socket, [this](const boost::system::error_code& error) {
        if(error) {
            spdlog::error("Error accepting socket: {}", error.message());
            return;
        }
        auto data = ReadPacket();
        if(data.empty()) {
            try {
                m_Socket.shutdown(boost::asio::socket_base::shutdown_both);
            } catch (...) {}
            m_Socket.close();
            Accept();
            return;
        }
        try {
            auto initPacket = PacketPairInit::FromJson({data.begin(), data.end()});
            if(!initPacket.has_value())
                throw std::runtime_error("Failed parsing packet.");
            if(AppInfo::CompareVersion(AppInfo::GetProtocolVersion(), initPacket->protoVersion) != 0)
                throw std::runtime_error("App version is incompatible with desktop version.");

            auto device = PairedDevice();
            device.pairingId = CryptUtils::Sha256(PlatformHelper::GetDeviceUUID() + initPacket->deviceUUID + m_ServerData.userName);
            device.pairingMethod = m_ServerData.method;
            device.deviceName = initPacket->deviceName;
            device.userName = m_ServerData.userName;
            device.encryptionKey = m_ServerData.encKey;

            device.ipAddress = initPacket->ipAddress;
            device.bluetoothAddress = m_ServerData.btAddress;
            device.cloudToken = initPacket->cloudToken;

            auto respPacket = PacketPairResponse();
            respPacket.pairingId = device.pairingId;
            respPacket.hostName = NetworkHelper::GetHostName();
            respPacket.hostOS = AppInfo::GetOperatingSystem();
            respPacket.hostAddress = NetworkHelper::GetSavedNetworkInterface().ipAddress;
            respPacket.hostPort = AppSettings::Get().unlockServerPort;
            respPacket.pairingMethod = device.pairingMethod;
            respPacket.macAddress = m_ServerData.macAddress;
            respPacket.userName = m_ServerData.userName;
            respPacket.password = m_ServerData.password;

            WritePacket(respPacket.ToJson().dump());
            PairedDevicesStorage::AddDevice(device);
            spdlog::info("Successfully paired device. (ID={}, Method={})", device.pairingId, PairingMethodUtils::ToString(device.pairingMethod));
        } catch(const std::exception& ex) {
            spdlog::error("Pairing server error: {}", ex.what());
            auto respPacket = PacketPairResponse();
            respPacket.errMsg = fmt::format("Error during pairing: {}", ex.what());
            WritePacket(respPacket.ToJson().dump());
        }
        try {
            m_Socket.shutdown(boost::asio::socket_base::shutdown_both);
        } catch (...) {}
        m_Socket.close();
        Accept();
    });
}

std::vector<uint8_t> PairingServer::ReadPacket() {
    boost::system::error_code ec;
    std::vector<uint8_t> lenBuffer{};
    lenBuffer.resize(sizeof(uint16_t));
    auto lenBytesRead = boost::asio::read(m_Socket, boost::asio::buffer(lenBuffer.data(), lenBuffer.size()), boost::asio::transfer_exactly(lenBuffer.size()), ec);
    if(lenBytesRead < sizeof(uint16_t)) {
        spdlog::warn("Error reading packet length.");
        return {};
    }

    uint16_t packetSize{};
    std::memcpy(&packetSize, lenBuffer.data(), sizeof(uint16_t));
    packetSize = ntohs(packetSize);
    if(packetSize == 0) {
        spdlog::warn("Empty packet received.");
        return {};
    }

    std::vector<uint8_t> buffer{};
    buffer.resize(packetSize);
    auto bytesRead = boost::asio::read(m_Socket, boost::asio::buffer(buffer.data(), buffer.size()), boost::asio::transfer_exactly(buffer.size()), ec);
    if(bytesRead < packetSize) {
        spdlog::warn("Error reading packet data. (Size={})", packetSize);
        return {};
    }

    auto res = CryptUtils::DecryptAESPacket(buffer, m_ServerData.encKey);
    if(res.result != PacketCryptResult::OK) {
        spdlog::warn("Error decrypting packet. (Code={})", (int)res.result);
        return {};
    }
    return res.data;
}

void PairingServer::WritePacket(const std::string &dataStr) {
    WritePacket(std::vector<uint8_t> {dataStr.begin(), dataStr.end()});
}

void PairingServer::WritePacket(const std::vector<uint8_t>& data) {
    auto res = CryptUtils::EncryptAESPacket(data, m_ServerData.encKey);
    if(res.result != PacketCryptResult::OK) {
        spdlog::warn("Error encrypting packet. (Size={}, Code={})", data.size(), (int)res.result);
        return;
    }

    uint16_t packetSize = htons(static_cast<uint16_t>(res.data.size()));
    boost::system::error_code ec;
    boost::asio::write(m_Socket, boost::asio::buffer(&packetSize, sizeof(uint16_t)), ec);
    boost::asio::write(m_Socket, boost::asio::buffer(res.data), ec);
}
