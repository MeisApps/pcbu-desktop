#include "IPCHelper.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <thread>

#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Windows.h>
#include <sddl.h>
#else
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace asio = boost::asio;

constexpr uint32_t MAX_JSON_SIZE = 1024 * 1024;
constexpr uint32_t MAX_BLOB_SIZE = 64 * 1024 * 1024;
constexpr uint32_t WAIT_SLICE_MS = 500;
constexpr uint32_t CONNECT_RETRY_MS = 50;

struct IPCHelper::Native {
  asio::io_context io{};
#ifdef WINDOWS
  HANDLE rawPipe{INVALID_HANDLE_VALUE};
  std::optional<asio::windows::stream_handle> stream;
  std::string pipeName{};
#else
  std::optional<asio::local::stream_protocol::acceptor> acceptor;
  std::optional<asio::local::stream_protocol::socket> socket;
  std::string fsPath{};
#endif
  bool isServer{};
  bool connected{};
};

IPCHelper::IPCHelper() : m_Native(std::make_unique<Native>()) {}

IPCHelper::~IPCHelper() {
  Close();
}

std::string IPCHelper::MakeName() {
  auto id = StringUtils::RandomString(16, false);
  if(id.empty())
    return {};
#ifdef WINDOWS
  return "\\\\.\\pipe\\pcbu_" + id;
#elif APPLE
  return "/tmp/pcbu_" + id + ".sock";
#else
  return "pcbu_" + id;
#endif
}

bool IPCHelper::Listen(const std::string &name) {
  Close();
  if(!m_Native)
    m_Native = std::make_unique<Native>();
  if(name.empty())
    return false;
  try {
#ifdef WINDOWS
    return CreatePipe(name);
#else
#ifdef APPLE
    unlink(name.c_str());
#endif
    m_Native->acceptor.emplace(m_Native->io);
    boost::system::error_code ec{};
    m_Native->acceptor->open(asio::local::stream_protocol(), ec);
    if(ec) {
      spdlog::error("Failed opening IPC acceptor. (Error={})", ec.message());
      return false;
    }
    m_Native->acceptor->bind(MakeEndpoint(name), ec);
    if(ec) {
      spdlog::error("Failed binding IPC socket. (Error={})", ec.message());
      return false;
    }
#ifdef APPLE
    chmod(name.c_str(), 0600);
    m_Native->fsPath = name;
#endif
    m_Native->acceptor->listen(1, ec);
    if(ec) {
      spdlog::error("Failed listening on IPC socket. (Error={})", ec.message());
      return false;
    }
    m_Native->isServer = true;
    return true;
#endif
  } catch(const std::exception &ex) {
    spdlog::error("Failed listening for elevator IPC. (Exception={})", ex.what());
  }
  return false;
}

bool IPCHelper::Accept(uint32_t timeoutMs, const std::function<bool()> &isRunning) {
  if(!m_Native)
    return false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

#ifdef WINDOWS
  if(m_Native->rawPipe == INVALID_HANDLE_VALUE || !m_Native->isServer)
    return false;
  while(true) {
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if(remain.count() <= 0 || !isRunning())
      return false;
    if(!WaitPipeClient(static_cast<uint32_t>(remain.count()), isRunning))
      return false;
    m_Native->stream.emplace(m_Native->io);
    boost::system::error_code ec{};
    m_Native->stream->assign(m_Native->rawPipe, ec);
    if(ec) {
      spdlog::error("Failed assigning pipe handle. (Error={})", ec.message());
      return false;
    }
    m_Native->rawPipe = INVALID_HANDLE_VALUE;
    m_Native->connected = true;
    if(IsPeerElevated())
      return true;
    spdlog::warn("Invalid elevator IPC peer. (Pid={})", GetPeerPid().value_or(0));
    m_Native->connected = false;
    m_Native->stream.reset();
    if(!CreatePipe(m_Native->pipeName))
      return false;
  }
#else
  if(!m_Native->acceptor)
    return false;
  while(true) {
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if(remain.count() <= 0 || !isRunning())
      return false;
    m_Native->socket.emplace(m_Native->io);
    bool ok = RunTimed(static_cast<uint32_t>(remain.count()), isRunning, [this](const std::function<void(bool)> &complete) {
      m_Native->acceptor->async_accept(*m_Native->socket, [complete](const boost::system::error_code &ec) { complete(!ec); });
    });
    if(!ok) {
      m_Native->socket.reset();
      return false;
    }
    SetNosigPipe(m_Native->socket->native_handle());
    m_Native->connected = true;
    if(IsPeerElevated()) {
      m_Native->acceptor.reset();
      return true;
    }
    spdlog::warn("Invalid elevator IPC peer. (Pid={})", GetPeerPid().value_or(0));
    m_Native->connected = false;
    m_Native->socket.reset();
  }
#endif
}

bool IPCHelper::Connect(const std::string &name, uint32_t timeoutMs, const std::function<bool()> &isRunning) {
  Close();
  if(!m_Native)
    m_Native = std::make_unique<Native>();
  if(name.empty())
    return false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

#ifdef WINDOWS
  auto pipeName = StringUtils::ToWideString(name);
  while(true) {
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if(!isRunning() || remain.count() <= 0)
      return false;
    auto slice = std::min(remain, std::chrono::milliseconds(CONNECT_RETRY_MS));
    if(WaitNamedPipeW(pipeName.c_str(), static_cast<DWORD>(slice.count()))) {
      HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
      if(pipe != INVALID_HANDLE_VALUE) {
        m_Native->stream.emplace(m_Native->io);
        boost::system::error_code ec{};
        m_Native->stream->assign(pipe, ec);
        if(ec) {
          CloseHandle(pipe);
          return false;
        }
        m_Native->isServer = false;
        m_Native->connected = true;
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_RETRY_MS));
  }
#else
  auto ep = MakeEndpoint(name);
  while(true) {
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if(!isRunning() || remain.count() <= 0)
      return false;
    m_Native->socket.emplace(m_Native->io);
    boost::system::error_code ec{};
    m_Native->socket->open(asio::local::stream_protocol(), ec);
    if(ec) {
      m_Native->socket.reset();
      return false;
    }
    auto slice = std::min(remain, std::chrono::milliseconds(CONNECT_RETRY_MS * 4));
    bool ok = RunTimed(static_cast<uint32_t>(slice.count()), isRunning, [this, ep](const std::function<void(bool)> &complete) {
      m_Native->socket->async_connect(ep, [complete](const boost::system::error_code &connectEc) { complete(!connectEc); });
    });
    if(ok) {
      SetNosigPipe(m_Native->socket->native_handle());
      m_Native->isServer = false;
      m_Native->connected = true;
      return true;
    }
    m_Native->socket.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_RETRY_MS));
  }
#endif
}

void IPCHelper::Close() {
  if(!m_Native)
    return;
  boost::system::error_code ec{};
#ifdef WINDOWS
  if(m_Native->stream)
    m_Native->stream->close(ec);
  m_Native->stream.reset();
  if(m_Native->rawPipe != INVALID_HANDLE_VALUE) {
    CloseHandle(m_Native->rawPipe);
    m_Native->rawPipe = INVALID_HANDLE_VALUE;
  }
  m_Native->pipeName.clear();
#else
  if(m_Native->socket) {
    m_Native->socket->shutdown(asio::socket_base::shutdown_both, ec);
    m_Native->socket->close(ec);
  }
  m_Native->socket.reset();
  if(m_Native->acceptor)
    m_Native->acceptor->close(ec);
  m_Native->acceptor.reset();
  if(!m_Native->fsPath.empty()) {
    unlink(m_Native->fsPath.c_str());
    m_Native->fsPath.clear();
  }
#endif
  m_Native->io.restart();
  m_Native->io.poll();
  m_Native->connected = false;
  m_Native->isServer = false;
}

bool IPCHelper::IsConnected() const {
  return m_Native && m_Native->connected;
}

std::optional<uint32_t> IPCHelper::GetPeerPid() const {
  if(!m_Native || !m_Native->connected)
    return {};
#ifdef WINDOWS
  if(!m_Native->stream)
    return {};
  ULONG pid{};
  HANDLE handle = m_Native->stream->native_handle();
  BOOL ok = m_Native->isServer ? GetNamedPipeClientProcessId(handle, &pid) : GetNamedPipeServerProcessId(handle, &pid);
  if(!ok)
    return {};
  return static_cast<uint32_t>(pid);
#elif LINUX
  if(!m_Native->socket)
    return {};
  struct ucred cred{};
  socklen_t len = sizeof(cred);
  if(getsockopt(m_Native->socket->native_handle(), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
    return {};
  return static_cast<uint32_t>(cred.pid);
#elif APPLE
  if(!m_Native->socket)
    return {};
  pid_t pid{};
  socklen_t len = sizeof(pid);
  if(getsockopt(m_Native->socket->native_handle(), SOL_LOCAL, LOCAL_PEERPID, &pid, &len) != 0)
    return {};
  return static_cast<uint32_t>(pid);
#else
  return {};
#endif
}

bool IPCHelper::IsPeerElevated() const {
  if(!m_Native || !m_Native->connected)
    return false;
#ifdef WINDOWS
  auto pid = GetPeerPid();
  if(!pid.has_value())
    return false;
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid.value());
  if(!process) {
    spdlog::error("Failed opening IPC peer process. (Pid={}, Code={})", pid.value(), GetLastError());
    return false;
  }
  HANDLE token{};
  if(!OpenProcessToken(process, TOKEN_QUERY, &token)) {
    spdlog::error("Failed opening IPC peer token. (Pid={}, Code={})", pid.value(), GetLastError());
    CloseHandle(process);
    return false;
  }
  TOKEN_ELEVATION elev{};
  DWORD size{};
  bool elevated = false;
  if(GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size))
    elevated = elev.TokenIsElevated != 0;
  else
    spdlog::error("Failed querying IPC peer token elevation. (Code={})", GetLastError());
  CloseHandle(token);
  CloseHandle(process);
  return elevated;
#elif LINUX
  if(!m_Native->socket)
    return false;
  struct ucred cred{};
  socklen_t len = sizeof(cred);
  if(getsockopt(m_Native->socket->native_handle(), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
    return false;
  return cred.uid == 0;
#elif APPLE
  if(!m_Native->socket)
    return false;
  uid_t uid{};
  gid_t gid{};
  if(getpeereid(m_Native->socket->native_handle(), &uid, &gid) != 0)
    return false;
  return uid == 0;
#else
  return false;
#endif
}

std::optional<IPCMessage> IPCHelper::ReadMessage(const std::function<bool()> &isRunning, uint32_t timeoutMs) {
  try {
    uint32_t jsonLen{};
    if(!ReadExact(&jsonLen, sizeof(jsonLen), timeoutMs, isRunning))
      return {};
    if(jsonLen == 0 || jsonLen > MAX_JSON_SIZE) {
      spdlog::error("Invalid IPC JSON size. (Size={})", jsonLen);
      m_Native->connected = false;
      return {};
    }
    std::string json(jsonLen, '\0');
    if(!ReadExact(json.data(), jsonLen, timeoutMs, isRunning))
      return {};
    uint32_t blobLen{};
    if(!ReadExact(&blobLen, sizeof(blobLen), timeoutMs, isRunning))
      return {};
    if(blobLen > MAX_BLOB_SIZE) {
      spdlog::error("Invalid IPC blob size. (Size={})", blobLen);
      m_Native->connected = false;
      return {};
    }
    IPCMessage msg{};
    msg.json = std::move(json);
    if(blobLen > 0) {
      msg.blob.resize(blobLen);
      if(!ReadExact(msg.blob.data(), blobLen, timeoutMs, isRunning))
        return {};
    }
    return msg;
  } catch(const std::exception &ex) {
    spdlog::error("Failed to read IPC message. (Exception={})", ex.what());
  }
  return {};
}

bool IPCHelper::WriteMessage(const IPCMessage &msg, const std::function<bool()> &isRunning, uint32_t timeoutMs) {
  try {
    if(msg.json.size() > MAX_JSON_SIZE || msg.blob.size() > MAX_BLOB_SIZE)
      return false;
    auto jsonLen = static_cast<uint32_t>(msg.json.size());
    auto blobLen = static_cast<uint32_t>(msg.blob.size());
    if(!WriteExact(&jsonLen, sizeof(jsonLen), timeoutMs, isRunning))
      return false;
    if(!WriteExact(msg.json.data(), msg.json.size(), timeoutMs, isRunning))
      return false;
    if(!WriteExact(&blobLen, sizeof(blobLen), timeoutMs, isRunning))
      return false;
    if(blobLen > 0 && !WriteExact(msg.blob.data(), msg.blob.size(), timeoutMs, isRunning))
      return false;
    return true;
  } catch(const std::exception &ex) {
    spdlog::error("Failed to write IPC message. (Exception={})", ex.what());
  }
  return false;
}

bool IPCHelper::ReadExact(void *buf, size_t n, uint32_t timeoutMs, const std::function<bool()> &isRunning) {
  if(!m_Native || !m_Native->connected || !buf)
    return false;
  if(n == 0)
    return true;
  bool ok = RunTimed(timeoutMs, isRunning, [this, buf, n](const std::function<void(bool)> &complete) {
#ifdef WINDOWS
    asio::async_read(*m_Native->stream, asio::buffer(buf, n), [complete](const boost::system::error_code &ec, std::size_t) { complete(!ec); });
#else
    asio::async_read(*m_Native->socket, asio::buffer(buf, n), [complete](const boost::system::error_code &ec, std::size_t) { complete(!ec); });
#endif
  });
  if(!ok)
    m_Native->connected = false;
  return ok;
}

bool IPCHelper::WriteExact(const void *buf, size_t n, uint32_t timeoutMs, const std::function<bool()> &isRunning) {
  if(!m_Native || !m_Native->connected)
    return n == 0;
  if(n == 0)
    return true;
  bool ok = RunTimed(timeoutMs, isRunning, [this, buf, n](const std::function<void(bool)> &complete) {
#ifdef WINDOWS
    asio::async_write(*m_Native->stream, asio::buffer(buf, n), [complete](const boost::system::error_code &ec, std::size_t) { complete(!ec); });
#else
    asio::async_write(*m_Native->socket, asio::buffer(buf, n), [complete](const boost::system::error_code &ec, std::size_t) { complete(!ec); });
#endif
  });
  if(!ok)
    m_Native->connected = false;
  return ok;
}

bool IPCHelper::RunTimed(uint32_t timeoutMs, const std::function<bool()> &isRunning,
                         const std::function<void(const std::function<void(bool)> &)> &startOp) {
  if(!m_Native)
    return false;
  std::optional<bool> result{};
  asio::steady_timer timer(m_Native->io);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

  auto complete = [&](bool ok) {
    if(result.has_value())
      return;
    result = ok;
    timer.cancel();
  };

  startOp(complete);

  std::function<void(boost::system::error_code)> tick;
  tick = [&](boost::system::error_code) {
    if(result.has_value())
      return;
    auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if(!isRunning() || remain.count() <= 0) {
      CancelOp();
      complete(false);
      return;
    }
    timer.expires_after(std::min(remain, std::chrono::milliseconds(WAIT_SLICE_MS)));
    timer.async_wait(tick);
  };
  tick({});

  m_Native->io.restart();
  m_Native->io.run();
  return result.value_or(false);
}

void IPCHelper::CancelOp() {
  if(!m_Native)
    return;
  boost::system::error_code ec{};
#ifdef WINDOWS
  if(m_Native->stream)
    m_Native->stream->cancel(ec);
#else
  if(m_Native->socket)
    m_Native->socket->cancel(ec);
  if(m_Native->acceptor)
    m_Native->acceptor->cancel(ec);
#endif
}

#ifdef WINDOWS
bool IPCHelper::CreatePipe(const std::string &name) {
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = FALSE;
  PSECURITY_DESCRIPTOR sd = nullptr;
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(A;;GA;;;OW)", SDDL_REVISION_1, &sd, nullptr)) {
    spdlog::error("Failed creating pipe security descriptor. (Code={})", GetLastError());
    return false;
  }
  sa.lpSecurityDescriptor = sd;
  auto pipeName = StringUtils::ToWideString(name);
  HANDLE pipe = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0, &sa);
  LocalFree(sd);
  if(pipe == INVALID_HANDLE_VALUE) {
    spdlog::error("Failed creating named pipe. (Code={})", GetLastError());
    return false;
  }
  m_Native->rawPipe = pipe;
  m_Native->pipeName = name;
  m_Native->isServer = true;
  return true;
}

bool IPCHelper::WaitPipeClient(uint32_t timeoutMs, const std::function<bool()> &isRunning) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if(!ev)
    return false;
  OVERLAPPED ov{};
  ov.hEvent = ev;
  BOOL ok = ConnectNamedPipe(m_Native->rawPipe, &ov);
  DWORD err = ok ? ERROR_SUCCESS : GetLastError();
  if(err == ERROR_IO_PENDING) {
    while(true) {
      auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
      if(!isRunning() || remain.count() <= 0) {
        CancelIoEx(m_Native->rawPipe, &ov);
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
        return false;
      }
      auto slice = std::min(remain, std::chrono::milliseconds(WAIT_SLICE_MS));
      auto wait = WaitForSingleObject(ev, static_cast<DWORD>(slice.count()));
      if(wait == WAIT_OBJECT_0)
        break;
      if(wait != WAIT_TIMEOUT) {
        CancelIoEx(m_Native->rawPipe, &ov);
        WaitForSingleObject(ev, INFINITE);
        CloseHandle(ev);
        return false;
      }
    }
    DWORD bytes{};
    if(!GetOverlappedResult(m_Native->rawPipe, &ov, &bytes, FALSE)) {
      CloseHandle(ev);
      return false;
    }
    err = ERROR_SUCCESS;
  }
  CloseHandle(ev);
  return err == ERROR_SUCCESS || err == ERROR_PIPE_CONNECTED;
}
#else
asio::local::stream_protocol::endpoint IPCHelper::MakeEndpoint(const std::string &name) {
#ifdef LINUX
  std::string path(1, '\0');
  path += name;
  return {path};
#else
  return {name};
#endif
}

void IPCHelper::SetNosigPipe(int fd) {
#ifdef APPLE
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
  (void)fd;
#endif
}
#endif
