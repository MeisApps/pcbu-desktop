#include "Shell.h"

#include "shell/ElevatorService.h"
#include "shell/LocalShell.h"

std::atomic<ElevatorService *> Shell::g_ElevatorService{};
std::function<void()> Shell::g_ElevatorLostHandler{};
std::mutex Shell::g_ElevatorLostMutex{};

void Shell::Init() {
  if(GetElevator() || LocalShell::IsRunningAsAdmin())
    return;
  g_ElevatorService.store(new ElevatorService(), std::memory_order_release);
}

void Shell::Destroy() {
  SetElevatorLostHandler({});
  delete g_ElevatorService.exchange(nullptr, std::memory_order_acq_rel);
}

void Shell::SetElevatorLostHandler(const std::function<void()> &handler) {
  std::unique_lock lock(g_ElevatorLostMutex);
  g_ElevatorLostHandler = handler;
}

ElevatorService *Shell::GetElevator() {
  return g_ElevatorService.load(std::memory_order_acquire);
}

bool Shell::HasAdmin() {
  auto elevator = GetElevator();
  return elevator ? elevator->IsRunning() : LocalShell::IsRunningAsAdmin();
}

ShellCmdResult Shell::RunCommand(const std::string &cmd) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::RunCommand(cmd);

  auto resp = Exec(*elevator, ElevatorCommandType::RUN_CMD, {cmd});
  if(resp.has_value())
    return resp.value().cmdResult;
  return {-1, "Elevator response timeout."};
}

bool Shell::CreateDir(const std::filesystem::path &path) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::CreateDir(path);
  return Exec(*elevator, ElevatorCommandType::CREATE_FILE, {path.string(), "true"}).has_value();
}

bool Shell::CreateFile(const std::filesystem::path &path) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::CreateFile(path);
  return Exec(*elevator, ElevatorCommandType::CREATE_FILE, {path.string(), "false"}).has_value();
}

bool Shell::Remove(const std::filesystem::path &path, bool recursive) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::Remove(path, recursive);
  return Exec(*elevator, ElevatorCommandType::REMOVE, {path.string(), recursive ? "true" : "false"}).has_value();
}

std::vector<uint8_t> Shell::ReadBytes(const std::filesystem::path &path) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::ReadBytes(path);

  auto resp = Exec(*elevator, ElevatorCommandType::READ_BYTES, {path.string()});
  if(resp.has_value())
    return std::move(resp.value().dataBytes);
  return {};
}

bool Shell::WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::WriteBytes(path, data);
  return Exec(*elevator, ElevatorCommandType::WRITE_BYTES, {path.string()}, data).has_value();
}

bool Shell::ProtectFile(const std::filesystem::path &path, bool enabled) {
  auto elevator = GetElevator();
  if(!elevator)
    return LocalShell::ProtectFile(path, enabled);
  return Exec(*elevator, ElevatorCommandType::PROTECT_FILE, {path.string(), enabled ? "true" : "false"}).has_value();
}

std::optional<ElevatorCommandResponse> Shell::Exec(ElevatorService &elevator, ElevatorCommandType type, std::vector<std::string> args,
                                                   std::vector<uint8_t> data) {
  auto req = ElevatorCommand();
  req.type = type;
  req.args = std::move(args);
  req.dataBytes = std::move(data);
  auto resp = elevator.ExecCommand(req);
  if(!resp.has_value() && !elevator.IsRunning())
    OnElevatorLost();
  if(!resp.has_value() || resp.value().isError)
    return {};
  return resp;
}

void Shell::OnElevatorLost() {
  std::function<void()> handler{};
  {
    std::unique_lock lock(g_ElevatorLostMutex);
    handler = std::move(g_ElevatorLostHandler);
    g_ElevatorLostHandler = {};
  }
  if(handler)
    handler();
}
