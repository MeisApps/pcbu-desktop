#include "BaseUnlockServer.h"

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

std::string BaseUnlockServer::GetUnlockInfoPacket() {
    // Unlock info
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
