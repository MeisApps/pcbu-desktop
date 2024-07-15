#include "UnlockHandler.h"
#include "storage/AppSettings.h"
#include "KeyScanner.h"
#include "I18n.h"

#include "connection/clients/TCPUnlockClient.h"
#include "connection/clients/BTUnlockClient.h"

#ifdef _WIN32
#include <Windows.h>
#define KEY_LEFTCTRL VK_LCONTROL
#define KEY_LEFTALT VK_LMENU
#endif
#ifdef LINUX
#include <linux/input.h>
#endif
#ifdef APPLE
#include <Carbon/Carbon.h>
#define KEY_LEFTCTRL kVK_Control
#define KEY_LEFTALT kVK_Option
#endif

UnlockHandler::UnlockHandler(const std::function<void(std::string)>& printMessage) {
    m_PrintMessage = printMessage;
}

UnlockResult UnlockHandler::GetResult(const std::string& authUser, const std::string& authProgram, std::atomic<bool> *isRunning) {
    auto appSettings = AppSettings::Get();
    std::vector<BaseUnlockServer *> servers{};
    for(const auto& device : PairedDevicesStorage::GetDevicesForUser(authUser)) {
        BaseUnlockServer *server{};
        switch (device.pairingMethod) {
            case PairingMethod::TCP:
                server = new TCPUnlockClient(device.ipAddress, 43298, device);
                break;
            case PairingMethod::BLUETOOTH:
                server = new BTUnlockClient(device.bluetoothAddress, device);
                break;
            /*case PairingMethod::CLOUD_TCP:
                server = new TCPUnlockServer("0.0.0.0", appSettings.unlockServerPort, device);
                break;*/
            default:
                break;
        }
        if(server == nullptr) {
            spdlog::error("Invalid pairing method.");
            continue;
        }
        server->SetUnlockInfo(authUser, authProgram);
        servers.emplace_back(server);
    }
    if(servers.empty()) {
        auto errorMsg = I18n::Get("error_not_paired", authUser);
        spdlog::error(errorMsg);
        m_PrintMessage(errorMsg);
        return UnlockResult(UnlockState::NOT_PAIRED_ERROR);
    }

    // Start servers
    std::vector<std::thread> threads{};
    AtomicUnlockResult currentResult{};
    std::atomic completed(0);
    std::mutex mutex{};
    std::condition_variable cv{};
    auto numServers = servers.size();
    for (auto server : servers) {
        threads.emplace_back([this, server, numServers, isRunning, &currentResult, &completed, &cv, &mutex]() {
            auto serverResult = RunServer(server, &currentResult, isRunning);
            completed.fetch_add(1);
            if(serverResult.state == UnlockState::SUCCESS)
                currentResult.store(serverResult);
            if(completed.load() == numServers) {
                if(currentResult.load().state != UnlockState::SUCCESS)
                    currentResult.store(serverResult);
                std::lock_guard l(mutex);
                cv.notify_one();
            }
        });
    }

    // Wait
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] {
        return completed.load() == numServers;
    });
    auto result = currentResult.load();

    // Cleanup
    for (auto& thread : threads) {
        if (thread.joinable())
            thread.join();
    }
    for(const auto server : servers)
        delete server;
    return result;
}

UnlockResult UnlockHandler::RunServer(BaseUnlockServer *server, AtomicUnlockResult *currentResult, std::atomic<bool> *isRunning) {
    if(!server->Start()) {
        auto errorMsg = I18n::Get("error_start_handler");
        spdlog::error(errorMsg);
        m_PrintMessage(errorMsg);
        return UnlockResult(UnlockState::START_ERROR);
    }

    m_PrintMessage(I18n::Get("wait_phone_connect"));
    auto keyScanner = KeyScanner();
    keyScanner.Start();

    auto state = UnlockState::UNKNOWN;
    auto startTime = Utils::GetCurrentTimeMs();
    auto isWaitingForConnection = true;
    auto isFutureCancel = false;
    while (true) {
        if(currentResult->load().state == UnlockState::SUCCESS || (isRunning != nullptr && !isRunning->load())) {
            state = UnlockState::CANCELED;
            isFutureCancel = true;
            break;
        }

        if(server->HasClient() && isWaitingForConnection) {
            m_PrintMessage(I18n::Get("wait_phone_unlock"));
            isWaitingForConnection = false;
        }
        state = server->PollResult();
        if(state != UnlockState::UNKNOWN)
            break;
        if(!server->HasClient() && Utils::GetCurrentTimeMs() - startTime > CRYPT_PACKET_TIMEOUT) {
            state = UnlockState::TIMEOUT;
            break;
        }
        if(keyScanner.GetKeyState(KEY_LEFTCTRL) && keyScanner.GetKeyState(KEY_LEFTALT)) {
            state = UnlockState::CANCELED;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    server->Stop();
    keyScanner.Stop();
    if(!isFutureCancel)
        m_PrintMessage(UnlockStateUtils::ToString(state));
    spdlog::info("Server result: {}", UnlockStateUtils::ToString(state));

    auto result = UnlockResult();
    result.state = state;
    result.device = server->GetDevice();
    result.password = server->GetResponseData().password;
    return result;
}
