#include "ElevatorService.h"

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/interprocess/permissions.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shell/IPCHelper.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <boost/process/v2/windows/creation_flags.hpp>
#include <sddl.h>

constexpr boost::process::v2::windows::process_creation_flags<CREATE_NO_WINDOW> create_no_window;
#endif

ElevatorService::ElevatorService() : m_Ctx(), m_SendQueue(), m_RecvQueue() {
  try {
    std::string base_name = StringUtils::RandomString(64, false);
    m_SendQueueName = "pcbu_elevator_send_" + base_name;
    m_RecvQueueName = "pcbu_elevator_recv_" + base_name;
    m_SharedMemName = "pcbu_elevator_shm_" + base_name;

    boost::interprocess::message_queue::remove(m_SendQueueName.c_str());
    boost::interprocess::message_queue::remove(m_RecvQueueName.c_str());
    boost::interprocess::shared_memory_object::remove(m_SharedMemName.c_str());

    boost::interprocess::permissions perm{};
#ifdef WINDOWS
    PSECURITY_DESCRIPTOR sd = nullptr;
    if(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;OW)", SDDL_REVISION_1, &sd, nullptr))
      perm.set_permissions(sd);
    else
      spdlog::warn("Failed setting permissions for interprocess communication. (Code={})", GetLastError());
#else
    perm.set_permissions(0600);
#endif

    m_SendQueue.emplace(boost::interprocess::create_only, m_SendQueueName.c_str(), 8192, 32768, perm);
    m_RecvQueue.emplace(boost::interprocess::create_only, m_RecvQueueName.c_str(), 8192, 32768, perm);
    m_SharedMem.emplace(boost::interprocess::create_only, m_SharedMemName.c_str(), 64 * 1048576); // 64 MiB

    auto ex = m_Ctx.get_executor();
    auto procPath = boost::dll::program_location().parent_path() / "pcbu_elevator";
    auto procCmd = procPath.string() + " " + m_SendQueueName + " " + m_RecvQueueName + " " + m_SharedMemName;
#ifdef WINDOWS
    m_Process = boost::process::process(
        ex, boost::process::v2::environment::find_executable("powershell"),
        {"-Command",
         fmt::format("$p = Start-Process '{}' -ArgumentList '{} {} {}' -Verb RunAs -WindowStyle Hidden -PassThru; $p.WaitForExit(); exit $p.ExitCode",
                     procPath.string(), m_SendQueueName, m_RecvQueueName, m_SharedMemName)},
        create_no_window);
    LocalFree(sd);
#elif LINUX
    m_Process = boost::process::process(ex, boost::process::v2::environment::find_executable("pkexec"), {procCmd});
#elif APPLE
    m_Process = boost::process::process(ex, boost::process::v2::environment::find_executable("osascript"),
                                        {"-e", fmt::format("do shell script \"{}\" with administrator privileges", procCmd)});
#endif
    spdlog::info("[ElevatorService] Elevator started.");
  } catch(const std::exception &ex) {
    spdlog::error("[ElevatorService] Failed to start elevator process. (Exception={})", ex.what());
  }
}

ElevatorService::~ElevatorService() {
  boost::interprocess::message_queue::remove(m_SendQueueName.c_str());
  boost::interprocess::message_queue::remove(m_RecvQueueName.c_str());
  boost::interprocess::shared_memory_object::remove(m_SharedMemName.c_str());

  boost::system::error_code ec{};
  if(m_Process.has_value() && m_Process.value().running(ec)) {
    ExecCommand(ElevatorCommand(ElevatorCommandType::QUIT));
    m_Process.value().terminate(ec);
    spdlog::info("[ElevatorService] Elevator stopped.");
  }
}

bool ElevatorService::IsRunning() {
  return m_Process.has_value() && m_Process.value().running();
}

std::optional<boost::interprocess::managed_shared_memory> &ElevatorService::GetSharedMemory() {
  return m_SharedMem;
}

std::optional<ElevatorCommandResponse> ElevatorService::ExecCommand(const ElevatorCommand &cmd) {
  std::unique_lock lock(m_Mutex);
  const auto isRunning = [this]() { return m_Process.has_value() && m_Process.value().running(); };
  if(!isRunning() || !m_SendQueue.has_value() || !m_RecvQueue.has_value() || !m_SharedMem.has_value()) {
    auto exitCode = m_Process.has_value() ? m_Process.value().exit_code() : 0;
    spdlog::error("[ElevatorService] The elevator process died. (Code={})", exitCode);
    return {};
  }

  spdlog::debug("[ElevatorService] Sending command... (Type={})", (int)cmd.type);
  IPCHelper::WriteString(m_SendQueue.value(), cmd.ToJson().dump(), isRunning);

  spdlog::debug("[ElevatorService] Reading command... (Type={})", (int)cmd.type);
  auto respStr = IPCHelper::ReadString(m_RecvQueue.value(), isRunning);
  if(!respStr.has_value()) {
    spdlog::error("[ElevatorService] Failed to read response from elevator. (Running={})", isRunning());
    return {};
  }
  auto resp = ElevatorCommandResponse::FromJson(respStr.value());
  if(!resp.has_value()) {
    spdlog::error("[ElevatorService] Failed to parse response from elevator. (CmdType={}, Str={})", (int)cmd.type, respStr.value());
    return {};
  }
  if(resp.value().type == ElevatorCommandResponseType::DATA && resp.value().dataLength > 0) {
    auto dataBytes = IPCHelper::ReadShmBytes(m_SharedMem.value(), resp.value().dataHandle, resp.value().dataLength);
    resp.value().dataBytes = dataBytes;
    if(dataBytes.empty()) {
      spdlog::error("[ElevatorService] Failed to read shared memory. (Size={})", resp.value().dataLength);
      return {};
    }
  }
  return resp;
}
