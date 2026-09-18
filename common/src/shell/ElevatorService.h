#ifndef ELEVATORSERVICE_H
#define ELEVATORSERVICE_H

#include <mutex>
#include <optional>

#include <boost/asio.hpp>
#include <boost/process.hpp>

#include "ElevatorCommands.h"
#include "IPCHelper.h"

class ElevatorService {
public:
  ElevatorService();
  ~ElevatorService();

  bool IsRunning();
  std::optional<ElevatorCommandResponse> ExecCommand(const ElevatorCommand &cmd);

private:
  bool IsProcessRunning();
  boost::asio::io_context m_Ctx;
  std::optional<boost::process::process> m_Process;
  std::optional<IPCHelper> m_Ipc;
  std::mutex m_Mutex{};
};

#endif
