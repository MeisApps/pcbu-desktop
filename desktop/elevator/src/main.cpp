#include <exception>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <nlohmann/json.hpp>

#include "shell/ElevatorCommands.h"
#include "shell/IPCHelper.h"
#include "shell/LocalShell.h"
#include "spdlog/spdlog.h"
#include "storage/LoggingSystem.h"

ElevatorCommand parse_command(boost::interprocess::managed_shared_memory &sharedMem, const std::string &cmdStr) {
  auto cmd = ElevatorCommand::FromJson(cmdStr);
  if(!cmd.has_value()) {
    spdlog::error("Failed parsing command. (Str={})", cmdStr);
    return {};
  }
  if(cmd.value().type == ElevatorCommandType::WRITE_BYTES && cmd.value().dataLength > 0) {
    auto dataBytes = IPCHelper::ReadShmBytes(sharedMem, cmd.value().dataHandle, cmd.value().dataLength);
    cmd.value().dataBytes = dataBytes;
    if(dataBytes.empty()) {
      spdlog::error("Failed to read shared memory. (Size={}))", cmd.value().dataLength);
      return {};
    }
  }
  return cmd.value();
}

void send_response(boost::interprocess::message_queue &sendQueue, const ElevatorCommandResponse &resp) {
  // spdlog::info("Sending response... (Type={})", (int)resp.type);
  IPCHelper::WriteString(sendQueue, resp.ToJson().dump());
}

void run_command(boost::interprocess::message_queue &sendQueue, boost::interprocess::managed_shared_memory &sharedMem, const ElevatorCommand &cmd) {
  try {
    switch(cmd.type) {
      case ElevatorCommandType::RUN_CMD: {
        auto result = LocalShell::RunCommand(cmd.args.at(0));
        send_response(sendQueue, ElevatorCommandResponse(result));
        break;
      }
      case ElevatorCommandType::READ_BYTES: {
        auto data = LocalShell::ReadBytes(cmd.args.at(0));
        auto shmHandle = IPCHelper::WriteShmBytes(sharedMem, data);
        send_response(sendQueue, ElevatorCommandResponse(shmHandle, data.size()));
        break;
      }
      case ElevatorCommandType::WRITE_BYTES: {
        auto result = LocalShell::WriteBytes(cmd.args.at(0), cmd.dataBytes);
        send_response(sendQueue, ElevatorCommandResponse(!result, ""));
        break;
      }
      case ElevatorCommandType::CREATE_FILE: {
        auto path = cmd.args.at(0);
        auto isDir = cmd.args.at(1) == "true";
        auto result = isDir ? LocalShell::CreateDir(path) : false;
        send_response(sendQueue, ElevatorCommandResponse(!result, ""));
        break;
      }
      case ElevatorCommandType::REMOVE_FILE: {
        auto path = cmd.args.at(0);
        auto isDir = cmd.args.at(1) == "true";
        auto result = isDir ? LocalShell::RemoveDir(path) : LocalShell::RemoveFile(path);
        send_response(sendQueue, ElevatorCommandResponse(!result, ""));
        break;
      }
      case ElevatorCommandType::PROTECT_FILE: {
        auto path = cmd.args.at(0);
        auto enabled = cmd.args.at(1) == "true";
        auto result = LocalShell::ProtectFile(path, enabled);
        send_response(sendQueue, ElevatorCommandResponse(!result, ""));
        break;
      }
      default:
        break;
    }
  } catch(const std::exception &ex) {
    spdlog::error("Failed executing command: {}", ex.what());
  }
}

int main(int argc, char *argv[]) {
  if(argc != 4) {
    spdlog::error("Invalid args.");
    return 1;
  }

  LoggingSystem::Init("elevator", false);
  boost::interprocess::message_queue recvQueue(boost::interprocess::open_only, argv[1]);
  boost::interprocess::message_queue sendQueue(boost::interprocess::open_only, argv[2]);
  boost::interprocess::managed_shared_memory sharedMem(boost::interprocess::open_only, argv[3]);

  std::optional<std::string> line{};
  ElevatorCommand cmd{};
  do {
    // spdlog::info("Reading command...");
    line = IPCHelper::ReadString(recvQueue);
    cmd = line.has_value() ? parse_command(sharedMem, line.value()) : ElevatorCommand();

    // spdlog::info("Running commmand {}...", (int)cmd.type);
    run_command(sendQueue, sharedMem, cmd);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while(cmd.type != ElevatorCommandType::QUIT);

  LoggingSystem::Destroy();
  return 0;
}
