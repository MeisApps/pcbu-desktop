#ifndef PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
#define PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include "nlohmann/json.hpp"

#include "handler/UnlockState.h"
#include "storage/PairedDevicesStorage.h"
#include "utils/Utils.h"
#include "utils/CryptUtils.h"

#ifdef WINDOWS
typedef unsigned long long SOCKET;
#else
#define SOCKET int
#endif

struct UnlockResponseData {
    std::string unlockToken;
    std::string password;
};

class BaseUnlockServer {
public:
    explicit BaseUnlockServer(const PairedDevice& device);
    virtual ~BaseUnlockServer();

    virtual bool Start() = 0;
    virtual void Stop() = 0;

    PairedDevice GetDevice();
    UnlockResponseData GetResponseData();
    [[nodiscard]] bool HasClient() const;

    void SetUnlockInfo(const std::string& authUser, const std::string& authProgram);
    UnlockState PollResult();

protected:
    static bool SetSocketBlocking(SOCKET socket, bool isBlocking);
    static std::vector<uint8_t> ReadPacket(SOCKET socket);
    static void WritePacket(SOCKET socket, const std::vector<uint8_t>& data);

    std::string GetUnlockInfoPacket();
    void OnResponseReceived(uint8_t *buffer, size_t buffer_size);

    bool m_IsRunning{};
    std::thread m_AcceptThread{};
    bool m_HasConnection{};
    std::string m_UserName{};

    std::atomic<UnlockState> m_UnlockState{};
    PairedDevice m_PairedDevice{};
    UnlockResponseData m_ResponseData{};

    std::string m_AuthUser{};
    std::string m_AuthProgram{};
    std::string m_UnlockToken{};
};

#endif //PAM_PCBIOUNLOCK_BASEUNLOCKSERVER_H
