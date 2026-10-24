#ifndef PCBU_CLI_INSTALLCOMMANDS_H
#define PCBU_CLI_INSTALLCOMMANDS_H

#include <string>
#include <vector>

class InstallCommands {
public:
  static int Status(const std::vector<std::string> &args);
  static int Install(const std::vector<std::string> &args);
  static int Reinstall(const std::vector<std::string> &args);
  static int Uninstall(const std::vector<std::string> &args);
  static int Update(const std::vector<std::string> &args);

private:
  InstallCommands() = default;

  static void PrintLog(const std::string &message);
};

#endif // PCBU_CLI_INSTALLCOMMANDS_H
