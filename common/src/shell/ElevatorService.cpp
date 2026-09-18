#include "ElevatorService.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include <boost/dll/runtime_symbol_info.hpp>
#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <boost/process/v2/windows/creation_flags.hpp>

constexpr boost::process::v2::windows::process_creation_flags<CREATE_NO_WINDOW> create_no_window;
#endif

constexpr uint32_t ACCEPT_TIMEOUT_MS = 300000;
constexpr uint32_t COMMAND_TIMEOUT_MS = 30000;
constexpr uint32_t SHUTDOWN_WAIT_MS = 1000;

ElevatorService::ElevatorService() : m_Ctx() {
  try {
    auto ipcName = IPCHelper::MakeName();
    if(ipcName.empty())
      throw std::runtime_error("Failed generating IPC name.");

    m_Ipc.emplace();
    if(!m_Ipc->Listen(ipcName))
      throw std::runtime_error("Failed listening for elevator IPC.");

    std::string pidStr{};
#ifdef WINDOWS
    pidStr = std::to_string(GetCurrentProcessId());
#else
    pidStr = std::to_string(getpid());
#endif
    auto ex = m_Ctx.get_executor();
    auto procPath = boost::dll::program_location().parent_path() / "pcbu_elevator";

#ifdef WINDOWS
    procPath += ".exe";
    auto psQuote = [](const std::string &s) { return StringUtils::Replace(s, "'", "''"); };
    m_Process = boost::process::process(
        ex, boost::process::v2::environment::find_executable("powershell"),
        {"-Command", fmt::format("$p = Start-Process -FilePath '{}' -ArgumentList @('{}','{}') -Verb RunAs -WindowStyle Hidden "
                                 "-PassThru; $p.WaitForExit(); exit $p.ExitCode",
                                 psQuote(procPath.string()), psQuote(ipcName), psQuote(pidStr))},
        create_no_window);
#elif LINUX
    m_Process = boost::process::process(ex, boost::process::v2::environment::find_executable("pkexec"), {procPath.string(), ipcName, pidStr});
#elif APPLE
    auto shellQuote = [](const std::string &s) { return "'" + StringUtils::Replace(s, "'", "'\\''") + "'"; };
    auto asEscape = [](const std::string &s) { return StringUtils::Replace(StringUtils::Replace(s, "\\", "\\\\"), "\"", "\\\""); };
    auto procCmd = fmt::format("{} {} {}", shellQuote(procPath.string()), shellQuote(ipcName), pidStr);
    m_Process = boost::process::process(ex, boost::process::v2::environment::find_executable("osascript"),
                                        {"-e", fmt::format("do shell script \"{}\" with administrator privileges", asEscape(procCmd))});
#endif

    if(!m_Ipc->Accept(ACCEPT_TIMEOUT_MS, [this]() { return IsProcessRunning(); }))
      throw std::runtime_error("Timed out waiting for elevator IPC.");
    spdlog::info("[ElevatorService] Elevator started.");
  } catch(const std::exception &ex) {
    spdlog::error("[ElevatorService] Failed to start elevator process. (Exception={})", ex.what());
    m_Ipc.reset();
  }
}

ElevatorService::~ElevatorService() {
  boost::system::error_code ec{};
  {
    std::unique_lock lock(m_Mutex);
    m_Ipc.reset();
  }
  if(!m_Process.has_value())
    return;

  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(SHUTDOWN_WAIT_MS);
  while(IsProcessRunning() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if(IsProcessRunning())
    m_Process.value().terminate(ec);
  spdlog::info("[ElevatorService] Elevator stopped.");
}

bool ElevatorService::IsProcessRunning() {
  boost::system::error_code ec{};
  return m_Process.has_value() && m_Process.value().running(ec);
}

bool ElevatorService::IsRunning() {
  return m_Ipc.has_value() && m_Ipc->IsConnected() && IsProcessRunning();
}

std::optional<ElevatorCommandResponse> ElevatorService::ExecCommand(const ElevatorCommand &cmd) {
  std::unique_lock lock(m_Mutex);
  const auto isRunning = [this]() { return IsRunning(); };
  if(!isRunning()) {
    auto exitCode = m_Process.has_value() ? m_Process.value().exit_code() : 0;
    spdlog::error("[ElevatorService] The elevator process died. (Code={})", exitCode);
    return {};
  }

  spdlog::debug("[ElevatorService] Sending command... (Type={})", (int)cmd.type);
  IPCMessage req{};
  req.json = cmd.ToJson().dump();
  req.blob = cmd.dataBytes;
  if(!m_Ipc->WriteMessage(req, isRunning, COMMAND_TIMEOUT_MS)) {
    spdlog::error("[ElevatorService] Failed to write command to elevator. (Type={})", (int)cmd.type);
    return {};
  }

  spdlog::debug("[ElevatorService] Reading response... (Type={})", (int)cmd.type);
  auto respMsg = m_Ipc->ReadMessage(isRunning, COMMAND_TIMEOUT_MS);
  if(!respMsg.has_value()) {
    spdlog::error("[ElevatorService] Failed to read response from elevator. (Running={})", isRunning());
    return {};
  }
  auto resp = ElevatorCommandResponse::FromJson(respMsg.value().json);
  if(!resp.has_value()) {
    spdlog::error("[ElevatorService] Failed to parse response from elevator. (CmdType={}, Str={})", (int)cmd.type, respMsg.value().json);
    return {};
  }
  resp.value().dataBytes = std::move(respMsg.value().blob);
  return resp;
}
