#include "ElevatorService.h"

#include <boost/asio.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/interprocess/permissions.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "shell/IPCHelper.h"
#include "utils/StringUtils.h"

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
    perm.set_permissions(0600);
    m_SendQueue.emplace(boost::interprocess::create_only, m_SendQueueName.c_str(), 8192, 32768, perm);
    m_RecvQueue.emplace(boost::interprocess::create_only, m_RecvQueueName.c_str(), 8192, 32768, perm);
    m_SharedMem.emplace(boost::interprocess::create_only, m_SharedMemName.c_str(), 64 * 1048576); // 64 MiB

    auto ex = m_Ctx.get_executor();
    auto procPath = boost::dll::program_location().parent_path() / "pcbu_elevator";
    auto procCmd = procPath.string() + " " + m_SendQueueName + " " + m_RecvQueueName + " " + m_SharedMemName;
#ifdef WINDOWS
#error ToDo
#elif LINUX
    m_Process = boost::process::process(ex, "/usr/bin/pkexec", {procCmd});
#elif APPLE
    m_Process = boost::process::process(ex, "/usr/bin/osascript", {"-e", "do shell script \"" + procCmd + "\" with administrator privileges"});
#endif
  } catch(const std::exception &ex) {
    spdlog::error("Failed to start elevator process. (Exception={})", ex.what());
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
  }
}

bool ElevatorService::IsRunning() {
  return m_Process.has_value() && m_Process.value().running();
}

std::optional<boost::interprocess::managed_shared_memory> &ElevatorService::GetSharedMemory() {
  return m_SharedMem;
}

std::optional<ElevatorCommandResponse> ElevatorService::ExecCommand(const ElevatorCommand &cmd) {
  const auto isRunning = [this]() { return m_Process.has_value() && m_Process.value().running(); };
  if(!isRunning() || !m_SendQueue.has_value() || !m_RecvQueue.has_value() || !m_SharedMem.has_value()) {
    auto exitCode = m_Process.has_value() ? m_Process.value().exit_code() : 0;
    spdlog::error("The elevator process died. (Code={})", exitCode);
    return {};
  }

  // spdlog::info("Sending command... (Type={})", (int)cmd.type);
  IPCHelper::WriteString(m_SendQueue.value(), cmd.ToJson().dump(), isRunning);

  // spdlog::info("Reading command... (Type={})", (int)cmd.type);
  auto respStr = IPCHelper::ReadString(m_RecvQueue.value(), isRunning);
  if(!respStr.has_value()) {
    spdlog::error("Failed to read response from elevator. (Running={})", isRunning());
    return {};
  }
  auto resp = ElevatorCommandResponse::FromJson(respStr.value());
  if(!resp.has_value()) {
    spdlog::error("Failed to parse response from elevator. (CmdType={}, Str={})", (int)cmd.type, respStr.value());
    return {};
  }
  if(resp.value().type == ElevatorCommandResponseType::DATA && resp.value().dataLength > 0) {
    auto dataBytes = IPCHelper::ReadShmBytes(m_SharedMem.value(), resp.value().dataHandle, resp.value().dataLength);
    resp.value().dataBytes = dataBytes;
    if(dataBytes.empty()) {
      spdlog::error("Failed to read shared memory. (Size={})", resp.value().dataLength);
      return {};
    }
  }
  return resp;
}
