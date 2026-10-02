#ifndef IPCHELPER_H
#define IPCHELPER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifndef WINDOWS
#include <boost/asio/local/stream_protocol.hpp>
#endif

struct IPCMessage {
  std::string json{};
  std::vector<uint8_t> blob{};
};

class IPCHelper {
public:
  static constexpr uint32_t NO_TIMEOUT = 0;
  static constexpr uint32_t MAX_JSON_SIZE = 1024 * 1024;
  static constexpr uint32_t MAX_BLOB_SIZE = 64 * 1024 * 1024;

  static bool IsTooLarge(const IPCMessage &msg);

  IPCHelper();
  ~IPCHelper();

  std::optional<std::string> Listen();
  bool Accept(uint32_t timeoutMs, const std::function<bool()> &isRunning = []() { return true; });
  bool Connect(const std::string &name, uint32_t timeoutMs, const std::function<bool()> &isRunning = []() { return true; });
  void Close();

  [[nodiscard]] bool IsConnected() const;
  [[nodiscard]] std::optional<uint32_t> GetPeerPid() const;
  [[nodiscard]] bool IsPeerElevated() const;

  std::optional<IPCMessage> ReadMessage(const std::function<bool()> &isRunning = []() { return true; }, uint32_t timeoutMs = 30000);
  bool WriteMessage(const IPCMessage &msg, const std::function<bool()> &isRunning = []() { return true; }, uint32_t timeoutMs = 30000);

private:
  struct Native;
  std::unique_ptr<Native> m_Native;

  static std::string MakeName();

  bool ReadExact(void *buf, size_t n, uint32_t timeoutMs, const std::function<bool()> &isRunning);
  bool WriteExact(const void *buf, size_t n, uint32_t timeoutMs, const std::function<bool()> &isRunning);

  bool RunTimed(uint32_t timeoutMs, const std::function<bool()> &isRunning, const std::function<void(const std::function<void(bool)> &)> &startOp);
  void CancelOp();

#ifdef WINDOWS
  bool CreatePipe(const std::string &name);
  bool WaitPipeClient(uint32_t timeoutMs, const std::function<bool()> &isRunning);
#else
  static boost::asio::local::stream_protocol::endpoint MakeEndpoint(const std::string &name);
  static void SetNosigPipe(int fd);
#endif
};

#endif
