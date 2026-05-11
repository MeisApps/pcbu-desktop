#include "Shell.h"

#include <spdlog/spdlog.h>

#include "shell/ElevatorService.h"
#include "shell/IPCHelper.h"
#include "shell/LocalShell.h"

bool Shell::g_IsInitialized{};
std::unique_ptr<ElevatorService> Shell::g_ElevatorService{};

void Shell::Init(bool useElevator) {
  if(g_IsInitialized)
    return;
  if(useElevator && !g_ElevatorService && !LocalShell::IsRunningAsAdmin())
    g_ElevatorService = std::make_unique<ElevatorService>();
  g_IsInitialized = true;
}

void Shell::Destroy() {
  g_IsInitialized = false;
  g_ElevatorService.reset();
}

bool Shell::HasAdmin() {
  return g_ElevatorService ? g_ElevatorService->IsRunning() : LocalShell::IsRunningAsAdmin();
}

ShellCmdResult Shell::RunCommand(const std::string &cmd) {
  if(!g_IsInitialized)
    return {-1, "Not initialized."};
  if(!g_ElevatorService)
    return LocalShell::RunCommand(cmd);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::RUN_CMD;
  req.args.emplace_back(cmd);
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::CMD_RESULT)
    return resp.value().cmdResult;
  return {-1, "Elevator response timeout."};
}

ShellCmdResult Shell::RunUserCommand(const std::string &cmd) {
  return LocalShell::RunUserCommand(cmd);
}

void Shell::SpawnCommand(const std::string &cmd) {
  return LocalShell::SpawnCommand(cmd);
}

bool Shell::CreateDir(const std::filesystem::path &path) {
  if(!g_IsInitialized)
    return false;
  if(!g_ElevatorService)
    return LocalShell::CreateDir(path);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::CREATE_FILE;
  req.args.emplace_back(path.string());
  req.args.emplace_back("true");
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::MESSAGE)
    return !resp.value().isError;
  return false;
}

bool Shell::RemoveDir(const std::filesystem::path &path) {
  if(!g_IsInitialized)
    return false;
  if(!g_ElevatorService)
    return LocalShell::RemoveDir(path);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::REMOVE_FILE;
  req.args.emplace_back(path.string());
  req.args.emplace_back("true");
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::MESSAGE)
    return !resp.value().isError;
  return false;
}

bool Shell::CreateFile(const std::filesystem::path &path) {
  if(!g_IsInitialized)
    return false;
  if(!g_ElevatorService)
    return LocalShell::CreateFile(path);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::CREATE_FILE;
  req.args.emplace_back(path.string());
  req.args.emplace_back("false");
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::MESSAGE)
    return !resp.value().isError;
  return false;
}

bool Shell::RemoveFile(const std::filesystem::path &path) {
  if(!g_IsInitialized)
    return false;
  if(!g_ElevatorService)
    return LocalShell::RemoveFile(path);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::REMOVE_FILE;
  req.args.emplace_back(path.string());
  req.args.emplace_back("false");
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::MESSAGE)
    return !resp.value().isError;
  return false;
}

std::vector<uint8_t> Shell::ReadBytes(const std::filesystem::path &path) {
  if(!g_IsInitialized)
    return {};
  if(!g_ElevatorService)
    return LocalShell::ReadBytes(path);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::READ_BYTES;
  req.args.emplace_back(path.string());
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::DATA)
    return resp.value().dataBytes;
  return {};
}

bool Shell::WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data) {
  if(!g_IsInitialized)
    return false;
  if(!g_ElevatorService || !g_ElevatorService->GetSharedMemory().has_value())
    return LocalShell::WriteBytes(path, data);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::WRITE_BYTES;
  req.args.emplace_back(path.string());

  req.dataHandle = IPCHelper::WriteShmBytes(g_ElevatorService->GetSharedMemory().value(), data);
  req.dataLength = data.size();

  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::MESSAGE)
    return !resp.value().isError;
  return false;
}

bool Shell::ProtectFile(const std::filesystem::path &path, bool enabled) {
  if(!g_IsInitialized)
    return false;
  if(!g_ElevatorService)
    return LocalShell::ProtectFile(path, enabled);

  auto req = ElevatorCommand();
  req.type = ElevatorCommandType::PROTECT_FILE;
  req.args.emplace_back(path.string());
  req.args.emplace_back(enabled ? "true" : "false");
  auto resp = g_ElevatorService->ExecCommand(req);
  if(resp.has_value() && resp.value().type == ElevatorCommandResponseType::MESSAGE)
    return !resp.value().isError;
  return false;
}
