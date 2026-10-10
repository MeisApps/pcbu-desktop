#include "AuthProcess.h"

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/process/v2/posix/bind_fd.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>
#include <spdlog/spdlog.h>

#include "platform/NetworkHelper.h"
#include "utils/I18n.h"

AuthProcess::AuthProcess(const std::string &userName, const std::function<void(const std::string &)> &onStatus,
                         const std::function<void(AuthResult)> &onFinished) {
  m_OnStatus = onStatus;
  m_OnFinished = onFinished;
  m_Thread = std::thread(&AuthProcess::Run, this, userName);
}

AuthProcess::~AuthProcess() {
  Cancel();
  if(m_Thread.joinable())
    m_Thread.join();
}

void AuthProcess::Cancel() {
  m_IsCanceled.store(true);
  std::lock_guard lock(m_CancelMutex);
  if(m_CancelFd != -1) {
    spdlog::info("Canceling pcbu_auth.");
    close(m_CancelFd);
    m_CancelFd = -1;
  }
}

bool AuthProcess::WaitForNetwork() {
  if(NetworkHelper::HasLANConnection())
    return true;
  m_OnStatus(I18n::Get("wait_network"));
  while(!m_IsCanceled.load()) {
    if(NetworkHelper::HasLANConnection())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

void AuthProcess::Run(const std::string &userName) {
  auto result = AuthResult();
  if(!WaitForNetwork()) {
    result.isCanceled = true;
    m_OnFinished(result);
    return;
  }

  int pipeFd[2]{};
  int cancelFd[2]{};
  if(pipe(pipeFd) != 0) {
    spdlog::error("Failed to create password pipe. (Code={})", errno);
    result.lastStatus = I18n::Get("error_unknown");
    m_OnFinished(result);
    return;
  }
  if(pipe(cancelFd) != 0) {
    spdlog::error("Failed to create cancel pipe. (Code={})", errno);
    close(pipeFd[0]);
    close(pipeFd[1]);
    result.lastStatus = I18n::Get("error_unknown");
    m_OnFinished(result);
    return;
  }
  for(auto fd : {pipeFd[0], pipeFd[1], cancelFd[0], cancelFd[1]})
    fcntl(fd, F_SETFD, FD_CLOEXEC);

  try {
    std::vector<std::string> args{userName, CANCEL_PIPE_ARG};
    boost::asio::io_context ctx{};
    boost::asio::readable_pipe outPipe{ctx};
    boost::process::v2::process proc(ctx, PCBU_AUTH_PATH, args, boost::process::v2::process_stdio{{}, outPipe, {}},
                                     boost::process::v2::posix::bind_fd(PASSWORD_PIPE, pipeFd[1]),
                                     boost::process::v2::posix::bind_fd(CANCEL_PIPE, cancelFd[0]));
    close(pipeFd[1]);
    pipeFd[1] = -1;
    close(cancelFd[0]);
    cancelFd[0] = -1;
    {
      std::lock_guard lock(m_CancelMutex);
      if(m_IsCanceled.load())
        close(cancelFd[1]);
      else
        m_CancelFd = cancelFd[1];
      cancelFd[1] = -1;
    }

    boost::system::error_code ec;
    std::vector<char> charBuffer(4096);
    std::string lineBuffer{};
    while(true) {
      auto n = outPipe.read_some(boost::asio::buffer(charBuffer), ec);
      if(ec)
        break;
      lineBuffer.append(charBuffer.data(), n);
      std::size_t pos{};
      while((pos = lineBuffer.find('\n')) != std::string::npos) {
        std::string line = lineBuffer.substr(0, pos);
        lineBuffer.erase(0, pos + 1);
        if(!line.empty() && line.back() == '\r')
          line.pop_back();
        if(line.empty())
          continue;
        result.lastStatus = line;
        m_OnStatus(line);
      }
    }

    char buffer[512]{};
    ssize_t bytesRead{};
    while((bytesRead = read(pipeFd[0], buffer, sizeof(buffer))) > 0)
      result.password.append(buffer, static_cast<size_t>(bytesRead));
    std::fill(std::begin(buffer), std::end(buffer), 0);

    proc.wait();
    result.exitCode = proc.exit_code();
  } catch(const std::exception &ex) {
    spdlog::error("Failed running pcbu_auth: {}", ex.what());
    result.lastStatus = I18n::Get("error_unknown");
  }
  {
    std::lock_guard lock(m_CancelMutex);
    if(m_CancelFd != -1) {
      close(m_CancelFd);
      m_CancelFd = -1;
    }
  }
  for(auto fd : {pipeFd[0], pipeFd[1], cancelFd[0], cancelFd[1]}) {
    if(fd != -1)
      close(fd);
  }

  result.isCanceled = m_IsCanceled.load();
  spdlog::info("pcbu_auth finished. (ExitCode={}, Canceled={})", result.exitCode, result.isCanceled);
  m_OnFinished(std::move(result));
}
