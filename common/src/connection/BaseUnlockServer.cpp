#include "BaseUnlockServer.h"

#include "SocketDefs.h"
#include "utils/StringUtils.h"

BaseUnlockServer::BaseUnlockServer(const PairedDevice& device) {
    m_PairedDevice = device;
    m_UnlockToken = StringUtils::RandomString(64);
    m_UnlockState = UnlockState::UNKNOWN;
}

BaseUnlockServer::~BaseUnlockServer() {
    if(m_AcceptThread.joinable())
        m_AcceptThread.join();
}

void BaseUnlockServer::SetUnlockInfo(const std::string& authUser, const std::string& authProgram) {
    m_AuthUser = authUser;
    m_AuthProgram = authProgram;
}

PairedDevice BaseUnlockServer::GetDevice() {
    return m_PairedDevice;
}

UnlockResponseData BaseUnlockServer::GetResponseData() {
    return m_ResponseData;
}

bool BaseUnlockServer::HasClient() const {
    return m_HasConnection;
}

UnlockState BaseUnlockServer::PollResult() {
    return m_UnlockState;
}

bool BaseUnlockServer::SetSocketBlocking(SOCKET socket, bool isBlocking) {
#ifdef WINDOWS
    if(isBlocking) {
        u_long mode = 0;
        return ioctlsocket(socket, FIONBIO, &mode) == 0;
    }
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    if(isBlocking) {
        int flags = fcntl(socket, F_GETFL, 0);
        if (flags == -1)
            return false;
        return fcntl(socket, F_SETFL, flags & ~O_NONBLOCK) != -1;
    }
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

std::vector<uint8_t> BaseUnlockServer::ReadPacket(SOCKET socket) {
    std::vector<uint8_t> lenBuffer{};
    lenBuffer.resize(sizeof(uint16_t));
    uint16_t lenBytesRead = 0;
    while (lenBytesRead < sizeof(uint16_t)) {
        int result = (int)read(socket, lenBuffer.data() + lenBytesRead, sizeof(uint16_t) - lenBytesRead);
        if (result <= 0) {
            spdlog::error("Reading length failed.");
            return {};
        }
        lenBytesRead += result;
    }

    uint16_t packetSize{};
    std::memcpy(&packetSize, lenBuffer.data(), sizeof(uint16_t));
    packetSize = ntohs(packetSize);
    if(packetSize == 0) {
        spdlog::error("Empty packet received.");
        return {};
    }

    std::vector<uint8_t> buffer{};
    buffer.resize(packetSize);
    uint16_t bytesRead = 0;
    while (bytesRead < packetSize) {
        int result = (int)read(socket, buffer.data() + bytesRead, packetSize - bytesRead);
        if (result <= 0) {
            spdlog::error("Reading data failed. (Len={})", packetSize);
            return {};
        }
        bytesRead += result;
    }
    return buffer;
}

void BaseUnlockServer::WritePacket(SOCKET socket, const std::vector<uint8_t>& data) {
    uint16_t packetSize = htons(static_cast<uint16_t>(data.size()));
    int bytesWritten = 0;
    while (bytesWritten < sizeof(uint16_t)) {
        int result = (int)write(socket, reinterpret_cast<const char*>(&packetSize) + bytesWritten, sizeof(uint16_t) - bytesWritten);
        if (result <= 0) {
            spdlog::error("Writing data len failed.");
            return;
        }
        bytesWritten += result;
    }
    bytesWritten = 0;
    while (bytesWritten < data.size()) {
        int result = (int)write(socket, reinterpret_cast<const char*>(data.data()) + bytesWritten, (int)data.size() - bytesWritten);
        if (result <= 0) {
            spdlog::error("Writing data failed. (Len={})", packetSize);
            return;
        }
        bytesWritten += result;
    }
}

std::string BaseUnlockServer::GetUnlockInfoPacket() {
    nlohmann::json encServerData = {
            {"authUser", m_AuthUser},
            {"authProgram", m_AuthProgram},
            {"unlockToken", m_UnlockToken}
    };
    auto encServerDataStr = encServerData.dump();
    auto cryptResult = CryptUtils::EncryptAESPacket({encServerDataStr.begin(), encServerDataStr.end()}, m_PairedDevice.encryptionKey);
    if(cryptResult.result != PacketCryptResult::OK) {
        spdlog::error("Encrypt error.");
        return {};
    }

    nlohmann::json serverData = {
            {"pairingId", m_PairedDevice.pairingId},
            {"encData", StringUtils::ToHexString(cryptResult.data)}
    };
    return serverData.dump();
}

void BaseUnlockServer::OnResponseReceived(uint8_t *buffer, size_t buffer_size) {
    if(buffer == nullptr || buffer_size == 0) {
        spdlog::error("Empty data received.", buffer_size);
        m_UnlockState = UnlockState::DATA_ERROR;
        return;
    }
    auto bufferVec = std::vector<uint8_t>(buffer_size);
    std::memcpy(bufferVec.data(), buffer, buffer_size);

    auto cryptResult = CryptUtils::DecryptAESPacket(bufferVec, m_PairedDevice.encryptionKey);
    if(cryptResult.result != PacketCryptResult::OK) {
        auto respStr = std::string((const char *)buffer, buffer_size);
        if(respStr == "CANCEL") {
            m_UnlockState = UnlockState::CANCELED;
            return;
        } else if(respStr == "NOT_PAIRED") {
            m_UnlockState = UnlockState::NOT_PAIRED_ERROR;
            return;
        } else if(respStr == "ERROR") {
            m_UnlockState = UnlockState::APP_ERROR;
            return;
        } else if(respStr == "TIME_ERROR") {
            m_UnlockState = UnlockState::TIME_ERROR;
            return;
        }

        switch (cryptResult.result) {
            case INVALID_TIMESTAMP: {
                spdlog::error("Invalid timestamp on AES data.");
                m_UnlockState = UnlockState::TIME_ERROR;
                break;
            }
            default: {
                spdlog::error("Invalid data received. (Size={})", buffer_size);
                m_UnlockState = UnlockState::DATA_ERROR;
                break;
            }
        }
        return;
    }

    // Parse data
    auto dataStr = std::string(cryptResult.data.begin(), cryptResult.data.end());
    try {
        auto json = nlohmann::json::parse(dataStr);
        auto response = UnlockResponseData();
        response.unlockToken = json["unlockToken"];
        response.password = json["password"];

        m_ResponseData = response;
        if(response.unlockToken == m_UnlockToken) {
            m_UnlockState = UnlockState::SUCCESS;
        } else {
            m_UnlockState = UnlockState::UNK_ERROR;
        }
    } catch(std::exception&) {
        spdlog::error("Error parsing response data.");
        m_UnlockState = UnlockState::DATA_ERROR;
    }
}
