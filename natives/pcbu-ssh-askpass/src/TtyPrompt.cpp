#include "TtyPrompt.h"

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <array>

TtyPrompt::TtyPrompt() {
  m_Fd = open("/dev/tty", O_RDWR);
  if(m_Fd < 0)
    return;
  if(tcgetattr(m_Fd, &m_Saved) != 0) {
    close(m_Fd);
    m_Fd = -1;
    return;
  }
  auto raw = m_Saved;
  raw.c_lflag &= ~ECHO;
  if(tcsetattr(m_Fd, TCSAFLUSH, &raw) != 0) {
    close(m_Fd);
    m_Fd = -1;
  }
}

TtyPrompt::~TtyPrompt() {
  if(m_Fd < 0)
    return;
  tcsetattr(m_Fd, TCSAFLUSH, &m_Saved);
  close(m_Fd);
}

bool TtyPrompt::IsOpen() const {
  return m_Fd >= 0;
}

std::optional<std::string> TtyPrompt::ReadLine() const {
  std::string line{};
  std::array<char, 4096> buf{};
  while(true) {
    auto n = read(m_Fd, buf.data(), buf.size());
    if(n <= 0)
      return {};
    for(ssize_t i = 0; i < n; i++) {
      if(buf[i] == '\n' || buf[i] == '\r')
        return line;
      line.push_back(buf[i]);
    }
  }
}

int TtyPrompt::ReadPassword(const std::string &prompt) {
  auto tty = TtyPrompt();
  if(!tty.IsOpen())
    return 1;

  auto display = prompt.empty() ? std::string("Password: ") : prompt;
  if(display.back() != ' ' && display.back() != '\n')
    display += ' ';
  if(write(tty.m_Fd, display.data(), display.size()) < 0)
    return 1;

  auto password = tty.ReadLine();
  write(tty.m_Fd, "\n", 1);
  if(!password)
    return 1;
  fmt::print("{}\n", *password);
  return 0;
}
