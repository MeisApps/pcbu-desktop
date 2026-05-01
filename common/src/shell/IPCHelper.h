#ifndef IPCHELPER_H
#define IPCHELPER_H

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class IPCHelper {
public:
  static std::optional<std::string>
  ReadString(boost::interprocess::message_queue &mq, const std::function<bool()> &isRunning = []() { return true; }, uint32_t timeoutMs = 30000);
  static bool WriteString(
      boost::interprocess::message_queue &mq, const std::string &str, const std::function<bool()> &isRunning = []() { return true; },
      uint32_t timeoutMs = 30000);

  static std::vector<uint8_t> ReadShmBytes(boost::interprocess::managed_shared_memory &sharedMem,
                                           boost::interprocess::managed_shared_memory::handle_t handle, size_t length);
  static boost::interprocess::managed_shared_memory::handle_t WriteShmBytes(boost::interprocess::managed_shared_memory &sharedMem,
                                                                            const std::vector<uint8_t> &data);

private:
  IPCHelper() = default;
};

#endif
