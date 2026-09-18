#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "shell/ElevatorCommands.h"
#include "shell/IPCHelper.h"
#include "shell/LocalShell.h"
#include "storage/LoggingSystem.h"

class ElevatorApp {
public:
  static int Run(int argc, char *argv[]);

private:
  ElevatorApp() = default;

  static bool SendResponse(IPCHelper &ipc, const ElevatorCommandResponse &resp);
  static void RunCommand(IPCHelper &ipc, const ElevatorCommand &cmd);
};

bool ElevatorApp::SendResponse(IPCHelper &ipc, const ElevatorCommandResponse &resp) {
  spdlog::debug("[Elevator] Sending response... (Type={})", (int)resp.type);
  IPCMessage msg{};
  msg.json = resp.ToJson().dump();
  msg.blob = resp.dataBytes;
  return ipc.WriteMessage(msg);
}

void ElevatorApp::RunCommand(IPCHelper &ipc, const ElevatorCommand &cmd) {
  try {
    switch(cmd.type) {
      case ElevatorCommandType::RUN_CMD: {
        auto result = LocalShell::RunCommand(cmd.args.at(0));
        SendResponse(ipc, ElevatorCommandResponse(result));
        break;
      }
      case ElevatorCommandType::READ_BYTES: {
        auto data = LocalShell::ReadBytes(cmd.args.at(0));
        SendResponse(ipc, ElevatorCommandResponse(std::move(data)));
        break;
      }
      case ElevatorCommandType::WRITE_BYTES: {
        auto result = LocalShell::WriteBytes(cmd.args.at(0), cmd.dataBytes);
        SendResponse(ipc, ElevatorCommandResponse(!result, ""));
        break;
      }
      case ElevatorCommandType::CREATE_FILE: {
        auto path = cmd.args.at(0);
        auto isDir = cmd.args.at(1) == "true";
        auto result = isDir ? LocalShell::CreateDir(path) : LocalShell::CreateFile(path);
        SendResponse(ipc, ElevatorCommandResponse(!result, ""));
        break;
      }
      case ElevatorCommandType::REMOVE_FILE: {
        auto path = cmd.args.at(0);
        auto isDir = cmd.args.at(1) == "true";
        auto result = isDir ? LocalShell::RemoveDir(path) : LocalShell::RemoveFile(path);
        SendResponse(ipc, ElevatorCommandResponse(!result, ""));
        break;
      }
      case ElevatorCommandType::PROTECT_FILE: {
        auto path = cmd.args.at(0);
        auto enabled = cmd.args.at(1) == "true";
        auto result = LocalShell::ProtectFile(path, enabled);
        SendResponse(ipc, ElevatorCommandResponse(!result, ""));
        break;
      }
      default:
        SendResponse(ipc, ElevatorCommandResponse(true, "Unknown command."));
        break;
    }
  } catch(const std::exception &ex) {
    spdlog::error("Failed executing command: {}", ex.what());
    SendResponse(ipc, ElevatorCommandResponse(true, ex.what()));
  }
}

int ElevatorApp::Run(int argc, char *argv[]) {
  if(argc != 3) {
    spdlog::error("Invalid args.");
    return 1;
  }

  LoggingSystem::Init("elevator", false, true);

  uint32_t expectedPid{};
  try {
    expectedPid = static_cast<uint32_t>(std::stoul(argv[2]));
    if(expectedPid == 0)
      throw std::runtime_error("");
  } catch(...) {
    spdlog::error("Invalid desktop pid.");
    LoggingSystem::Destroy();
    return 1;
  }

  IPCHelper ipc{};
  if(!ipc.Connect(argv[1], 30000)) {
    spdlog::error("Failed connecting to desktop IPC.");
    LoggingSystem::Destroy();
    return 1;
  }

  auto peerPid = ipc.GetPeerPid();
  if(!peerPid.has_value() || peerPid.value() != expectedPid) {
    spdlog::error("Elevator IPC peer mismatch. (Pid={})", peerPid.value_or(0));
    LoggingSystem::Destroy();
    return 1;
  }

  ElevatorCommandType cmdType{};
  do {
    spdlog::debug("[Elevator] Reading command...");
    auto msg = ipc.ReadMessage();
    if(!msg.has_value())
      break;

    auto cmd = ElevatorCommand::FromJson(msg.value().json);
    if(!cmd.has_value()) {
      spdlog::error("Failed parsing command. (Str={})", msg.value().json);
      SendResponse(ipc, ElevatorCommandResponse(true, "Invalid command."));
      continue;
    }
    cmdType = cmd.value().type;
    cmd.value().dataBytes = std::move(msg.value().blob);
    spdlog::debug("[Elevator] Running command {}...", (int)cmd.value().type);
    if(cmd.value().type == ElevatorCommandType::QUIT)
      break;
    RunCommand(ipc, cmd.value());
  } while(cmdType != ElevatorCommandType::QUIT);

  LoggingSystem::Destroy();
  return 0;
}

int main(int argc, char *argv[]) {
  return ElevatorApp::Run(argc, argv);
}
