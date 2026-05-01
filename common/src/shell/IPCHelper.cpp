#include "IPCHelper.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

std::optional<std::string> IPCHelper::ReadString(boost::interprocess::message_queue &mq, const std::function<bool()> &isRunning, uint32_t timeoutMs) {
  try {
    std::vector<char> buffer(mq.get_max_msg_size());
    boost::interprocess::message_queue::size_type size{};
    unsigned int priority{};

    auto deadline = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(timeoutMs);
    while(boost::posix_time::microsec_clock::universal_time() < deadline) {
      auto readTimeout = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(2000);
      if(mq.timed_receive(buffer.data(), buffer.size(), size, priority, readTimeout)) {
        if(size < sizeof(uint32_t))
          throw std::runtime_error("Invalid message");
        uint32_t len{};
        std::memcpy(&len, buffer.data(), sizeof(len));
        if(size != sizeof(len) + len)
          throw std::runtime_error("Size mismatch");
        return std::string(buffer.data() + sizeof(len), len);
      }
      if(!isRunning()) {
        return {};
      }
    }
  } catch(const std::exception &ex) {
    spdlog::error("Failed to read IPC string. (Exception={})", ex.what());
  }
  return {};
}

bool IPCHelper::WriteString(boost::interprocess::message_queue &mq, const std::string &str, const std::function<bool()> &isRunning,
                            uint32_t timeoutMs) {
  try {
    auto len = static_cast<uint32_t>(str.size());
    std::vector<char> buffer(sizeof(len) + str.size());
    std::memcpy(buffer.data(), &len, sizeof(len));
    std::memcpy(buffer.data() + sizeof(len), str.data(), str.size());
    auto deadline = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(timeoutMs);
    while(boost::posix_time::microsec_clock::universal_time() < deadline) {
      auto writeTimeout = boost::posix_time::microsec_clock::universal_time() + boost::posix_time::milliseconds(2000);
      if(mq.timed_send(buffer.data(), buffer.size(), 0, writeTimeout))
        return true;
      if(!isRunning())
        return false;
    }
  } catch(const std::exception &ex) {
    spdlog::error("Failed to write IPC string. (Exception={})", ex.what());
  }
  return false;
}

std::vector<uint8_t> IPCHelper::ReadShmBytes(boost::interprocess::managed_shared_memory &sharedMem,
                                             boost::interprocess::managed_shared_memory::handle_t handle, size_t length) {
  try {
    auto shmPtr = (uint8_t *)sharedMem.get_address_from_handle(handle);
    std::vector<uint8_t> result(length);
    result.resize(length);
    std::memcpy(result.data(), shmPtr, length);
    sharedMem.deallocate(shmPtr);
    return result;
  } catch(const std::exception &ex) {
    spdlog::error("Failed reading from shared memory. (Size={}, Exception={})", length, ex.what());
  }
  return {};
}

boost::interprocess::managed_shared_memory::handle_t IPCHelper::WriteShmBytes(boost::interprocess::managed_shared_memory &sharedMem,
                                                                              const std::vector<uint8_t> &data) {
  try {
    auto shmPtr = static_cast<uint8_t *>(sharedMem.allocate(data.size()));
    std::memcpy(shmPtr, data.data(), data.size());
    return sharedMem.get_handle_from_address(shmPtr);
  } catch(const std::exception &ex) {
    spdlog::error("Failed writing to shared memory. (Size={}, Exception={})", data.size(), ex.what());
  }
  return {};
}
