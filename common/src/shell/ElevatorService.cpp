#include "ElevatorService.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/process/v2/pid.hpp>
#include <spdlog/spdlog.h>

#include "utils/I18n.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <boost/process/v2/windows/creation_flags.hpp>

constexpr boost::process::v2::windows::process_creation_flags<CREATE_NO_WINDOW> create_no_window;
#endif

constexpr uint32_t ACCEPT_TIMEOUT_MS = 300000;
constexpr uint32_t COMMAND_TIMEOUT_MS = 30000;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 600000;
constexpr uint32_t SHUTDOWN_WAIT_MS = 1000;

ElevatorService::ElevatorService() : m_Ctx() {
  try {
    auto ipcName = m_Ipc.Listen();
    if(!ipcName.has_value())
      throw std::runtime_error("Failed listening for elevator IPC.");

    auto pidStr = std::to_string(boost::process::v2::current_pid());
    auto ex = m_Ctx.get_executor();
    auto procPath = boost::dll::program_location().parent_path() / "pcbu_elevator";

#ifdef WINDOWS
    procPath += ".exe";
    auto psQuote = [](const std::string &s) { return StringUtils::Replace(s, "'", "''"); };
    m_Process = boost::process::process(
        ex, boost::process::v2::environment::find_executable("powershell"),
        {"-Command", fmt::format("$p = Start-Process -FilePath '{}' -ArgumentList @('{}','{}') -Verb RunAs -WindowStyle Hidden "
                                 "-PassThru; $p.WaitForExit(); exit $p.ExitCode",
                                 psQuote(procPath.string()), psQuote(ipcName.value()), psQuote(pidStr))},
        create_no_window);
#elif LINUX
    m_Process = boost::process::process(ex, boost::process::v2::environment::find_executable("pkexec"), {procPath.string(), ipcName.value(), pidStr});
#elif APPLE
    auto asEscape = [](const std::string &s) { return StringUtils::Replace(StringUtils::Replace(s, "\\", "\\\\"), "\"", "\\\""); };
    auto procCmd = fmt::format("{} {} {}", StringUtils::ShellQuote(procPath.string()), StringUtils::ShellQuote(ipcName.value()), pidStr);
    m_Process = boost::process::process(ex, "/usr/bin/osascript",
                                        {"-e", fmt::format("do shell script \"{}\" with prompt \"{}\" with administrator privileges",
                                                           asEscape(procCmd), asEscape(I18n::Get("elevator_prompt", I18n::Get("product_name"))))});
#endif

    if(!m_Ipc.Accept(ACCEPT_TIMEOUT_MS, [this]() { return IsProcessRunning(); }))
      throw std::runtime_error("Timed out waiting for elevator IPC.");
    spdlog::info("[ElevatorService] Elevator started.");
  } catch(const std::exception &ex) {
    spdlog::error("[ElevatorService] Failed to start elevator process. (Exception={})", ex.what());
    m_Ipc.Close();
    if(IsProcessRunning()) {
      boost::system::error_code ec{};
      m_Process.value().terminate(ec);
    }
  }
}

ElevatorService::~ElevatorService() {
  boost::system::error_code ec{};
  {
    std::unique_lock lock(m_Mutex);
    m_Ipc.Close();
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
  std::unique_lock lock(m_Mutex);
  return IsRunningUnlocked();
}

bool ElevatorService::IsRunningUnlocked() {
  return m_Ipc.IsConnected() && IsProcessRunning();
}

std::optional<ElevatorCommandResponse> ElevatorService::ExecCommand(const ElevatorCommand &cmd) {
  std::unique_lock lock(m_Mutex);
  const auto isRunning = [this]() { return IsRunningUnlocked(); };
  if(!isRunning()) {
    auto exitCode = m_Process.has_value() ? m_Process.value().exit_code() : 0;
    spdlog::error("[ElevatorService] The elevator process died. (Code={}, {})", exitCode, cmd.ToString());
    return {};
  }

  auto msg = cmd.ToMessage();
  if(IPCHelper::IsTooLarge(msg)) {
    spdlog::error("[ElevatorService] Command too large. ({}, JsonSize={})", cmd.ToString(), msg.json.size());
    return {};
  }

  spdlog::debug("[ElevatorService] Sending command... ({})", cmd.ToString());
  if(!m_Ipc.WriteMessage(msg, isRunning, COMMAND_TIMEOUT_MS)) {
    spdlog::error("[ElevatorService] Failed to write command to elevator. ({})", cmd.ToString());
    m_Ipc.Close();
    return {};
  }

  spdlog::debug("[ElevatorService] Reading response... (Type={})", ElevatorCommand::GetTypeName(cmd.type));
  auto respMsg = m_Ipc.ReadMessage(isRunning, RESPONSE_TIMEOUT_MS);
  if(!respMsg.has_value()) {
    spdlog::error("[ElevatorService] Failed to read response from elevator. (Running={}, {})", isRunning(), cmd.ToString());
    m_Ipc.Close();
    return {};
  }
  auto resp = ElevatorCommandResponse::FromMessage(std::move(respMsg.value()));
  if(!resp.has_value())
    spdlog::error("[ElevatorService] Invalid response from elevator. ({})", cmd.ToString());
  else if(resp.value().isError)
    spdlog::error("[ElevatorService] Elevator command failed. ({}, {})", cmd.ToString(), resp.value().ToString());
  else
    spdlog::debug("[ElevatorService] Received response. (Type={}, {})", ElevatorCommand::GetTypeName(cmd.type), resp.value().ToString());
  return resp;
}
