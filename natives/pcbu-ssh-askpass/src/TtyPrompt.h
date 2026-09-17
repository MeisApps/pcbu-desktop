#ifndef PCBU_DESKTOP_TTYPROMPT_H
#define PCBU_DESKTOP_TTYPROMPT_H

#include <optional>
#include <string>
#include <termios.h>

class TtyPrompt {
public:
  TtyPrompt();
  ~TtyPrompt();
  TtyPrompt(const TtyPrompt &) = delete;
  TtyPrompt &operator=(const TtyPrompt &) = delete;

  static int ReadPassword(const std::string &prompt);

private:
  [[nodiscard]] bool IsOpen() const;
  [[nodiscard]] std::optional<std::string> ReadLine() const;

  int m_Fd{-1};
  termios m_Saved{};
};

#endif // PCBU_DESKTOP_TTYPROMPT_H
