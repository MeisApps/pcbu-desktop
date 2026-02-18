#ifndef ELEVATORSERVICE_H
#define ELEVATORSERVICE_H

#include <optional>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/process.hpp>

#include "ElevatorCommands.h"

class ElevatorService {
public:
  ElevatorService();
  ~ElevatorService();

  bool IsRunning();
  std::optional<boost::interprocess::managed_shared_memory> &GetSharedMemory();
  std::optional<ElevatorCommandResponse> ExecCommand(const ElevatorCommand &cmd);

private:
  boost::asio::io_context m_Ctx;
  std::optional<boost::process::process> m_Process;

  std::optional<boost::interprocess::message_queue> m_SendQueue;
  std::optional<boost::interprocess::message_queue> m_RecvQueue;
  std::optional<boost::interprocess::managed_shared_memory> m_SharedMem;

  std::string m_SendQueueName{};
  std::string m_RecvQueueName{};
  std::string m_SharedMemName{};
};

#endif
