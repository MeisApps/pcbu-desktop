#include "UnlockHandler.h"

#include <algorithm>

#include "KeyScanner.h"
#include "connection/unlock/clients/BTUnlockClient.h"
#include "connection/unlock/clients/CloudUnlockClient.h"
#include "connection/unlock/clients/TCPUnlockClient.h"
#include "connection/unlock/servers/TCPUnlockServer.h"
#include "storage/AppSettings.h"

#ifdef WINDOWS
#include <Windows.h>
#define KEY_LEFTCTRL VK_LCONTROL
#define KEY_LEFTALT VK_LMENU
#elif LINUX
#include <linux/input.h>
#elif APPLE
#include <Carbon/Carbon.h>
#define KEY_LEFTCTRL kVK_Control
#define KEY_LEFTALT kVK_Option
#endif

UnlockHandler::UnlockHandler(const std::function<void(std::string)> &printMessage) {
  m_PrintMessage = printMessage;
}

void UnlockHandler::PrintStatus(const BaseUnlockConnection *owner, UnlockPhase phase) {
  std::lock_guard lock(m_StatusMutex);
  if(m_StatusDone)
    return;
  if(phase < m_StatusPhase && owner != m_StatusOwner)
    return;
  m_StatusPhase = phase;
  m_StatusOwner = owner;
  auto message = UnlockPhaseUtils::ToString(phase);
  if(message.empty() || message == m_StatusMessage)
    return;
  m_StatusMessage = message;
  m_PrintMessage(message);
}

void UnlockHandler::PrintStatus(const std::string &message) {
  std::lock_guard lock(m_StatusMutex);
  if(m_StatusDone)
    return;
  m_StatusDone = true;
  m_StatusMessage = message;
  m_PrintMessage(message);
}

UnlockResult UnlockHandler::GetResult(const std::string &authUser, const std::string &authProgram, const std::vector<std::string> &deviceIds,
                                      std::atomic<bool> *isRunning) {
  auto settings = AppSettings::Get();
  auto devices = PairedDevicesStorage::GetDevicesForUser(authUser);
  auto hasTCPServer = false;
  if(!deviceIds.empty())
    std::erase_if(devices, [&](const PairedDevice &device) { return std::ranges::find(deviceIds, device.id) == deviceIds.end(); });

  UDPUnlockBroadcaster *udpBroadcaster{};
  std::vector<BaseUnlockConnection *> connections{};
  for(const auto &device : devices) {
    BaseUnlockConnection *connection{};
    switch(device.pairingMethod) {
      case PairingMethod::TCP:
        connection = new TCPUnlockClient(device.ipAddress, device.tcpPort, device);
        break;
      case PairingMethod::BLUETOOTH:
        connection = new BTUnlockClient(device.bluetoothAddress, device);
        break;
      case PairingMethod::MANUAL_UDP:
      case PairingMethod::UDP: {
        if(udpBroadcaster == nullptr)
          udpBroadcaster = new UDPUnlockBroadcaster();
        auto port = device.pairingMethod == PairingMethod::UDP ? device.udpPort : device.udpManualPort;
        udpBroadcaster->AddDevice(device.id, port, device.pairingMethod == PairingMethod::MANUAL_UDP);
        hasTCPServer = true;
        continue;
      }
      case PairingMethod::CLOUD:
        connection = new CloudUnlockClient(device);
        break;
      default: {
        spdlog::error("Invalid pairing method.");
        continue;
      }
    }
    if(connection != nullptr) {
      connection->SetUnlockInfo(authUser, authProgram);
      connections.emplace_back(connection);
    }
  }
  BaseUnlockConnection *udpServer{};
  if(hasTCPServer) {
    auto server = new TCPUnlockServer();
    server->SetUnlockInfo(authUser, authProgram);
    udpServer = server;
    connections.emplace_back(server);
  }
  if(connections.empty()) {
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
  auto numServers = connections.size();
  threads.reserve(numServers);
  for(auto connection : connections) {
    threads.emplace_back([this, connection, numServers, isRunning, &currentResult, &completed, &cv, &mutex, udpBroadcaster, udpServer]() {
      auto serverResult = RunServer(connection, connection == udpServer ? udpBroadcaster : nullptr, &currentResult, isRunning);
      if(serverResult.state == UnlockState::SUCCESS)
        currentResult.store(serverResult);
      auto isLast = completed.fetch_add(1) + 1 == numServers;
      if(serverResult.state == UnlockState::SUCCESS)
        PrintStatus(UnlockStateUtils::ToString(serverResult.state));
      if(isLast) {
        if(currentResult.load().state != UnlockState::SUCCESS) {
          currentResult.store(serverResult);
          PrintStatus(UnlockStateUtils::ToString(serverResult.state));
        }
        std::lock_guard l(mutex);
        cv.notify_one();
      }
    });
  }

  // UDP Broadcast
  if(udpBroadcaster) {
    udpBroadcaster->Start();
  }

  // Wait
  {
    std::unique_lock lock(mutex);
    cv.wait(lock, [&] { return completed.load() == numServers; });
  }
  auto result = currentResult.load();

  // Cleanup
  if(udpBroadcaster) {
    udpBroadcaster->Stop();
    delete udpBroadcaster;
  }
  for(auto &thread : threads) {
    if(thread.joinable())
      thread.join();
  }
  for(const auto connection : connections)
    delete connection;
  return result;
}

UnlockResult UnlockHandler::RunServer(BaseUnlockConnection *connection, UDPUnlockBroadcaster *udpBroadcaster, AtomicUnlockResult *currentResult,
                                      std::atomic<bool> *isRunning) {
  if(!connection->Start()) {
    spdlog::error(I18n::Get("error_start_handler"));
    return UnlockResult(UnlockState::START_ERROR);
  }

  auto keyScanner = KeyScanner();
  keyScanner.Start();

  auto state = UnlockState::UNKNOWN;
  auto startTime = Utils::GetCurrentTimeMs();
  auto isBroadcasting = true;
  while(true) {
    if(currentResult->load().state == UnlockState::SUCCESS || (isRunning != nullptr && !isRunning->load())) {
      state = UnlockState::CANCELED;
      break;
    }
    auto phase = connection->GetPhase();
    PrintStatus(connection, phase);
    if(phase == UnlockPhase::PHONE_UNLOCKING && isBroadcasting) {
      isBroadcasting = false;
      if(udpBroadcaster)
        udpBroadcaster->Stop();
    }

    state = connection->PollResult();
    if(state != UnlockState::UNKNOWN)
      break;
    if(phase != UnlockPhase::PHONE_UNLOCKING && Utils::GetCurrentTimeMs() - startTime > CRYPT_PACKET_TIMEOUT) {
      state = UnlockState::TIMEOUT;
      break;
    }
    if(keyScanner.GetKeyState(KEY_LEFTCTRL) && keyScanner.GetKeyState(KEY_LEFTALT)) {
      state = UnlockState::CANCELED;
      break;
    }

    if(phase != UnlockPhase::PHONE_UNLOCKING && !isBroadcasting) {
      isBroadcasting = true;
      if(udpBroadcaster)
        udpBroadcaster->Start();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  if(state != UnlockState::SUCCESS)
    PrintStatus(connection, UnlockPhase::FINISHED);
  connection->Stop();
  keyScanner.Stop();
  spdlog::info("Connection result: {}", UnlockStateUtils::ToString(state));

  auto pwDec = CryptUtils::DecryptAES(connection->GetDevice().passwordEnc, connection->GetResponseData().passwordKey);
  if(!pwDec.has_value() && state == UnlockState::SUCCESS) {
    auto errorMsg = I18n::Get("error_password_decrypt");
    spdlog::error(errorMsg);
    PrintStatus(errorMsg);
    return UnlockResult(UnlockState::DATA_ERROR);
  }

  auto result = UnlockResult();
  result.state = state;
  result.device = connection->GetDevice();
  result.password = pwDec.has_value() ? pwDec.value() : "";
  if(state == UnlockState::SUCCESS)
    result.passwordKey = connection->GetResponseData().passwordKey;
  return result;
}
