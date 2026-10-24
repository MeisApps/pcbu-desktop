#ifndef PCBU_CLI_CLI_H
#define PCBU_CLI_CLI_H

#include <functional>
#include <map>
#include <string>
#include <vector>

class Cli {
public:
  static int Run(int argc, char *argv[]);

private:
  Cli() = default;

  static std::map<std::string, std::function<int(const std::vector<std::string> &)>> GetCommands();
  static void CheckEnvironment();
  static void PrintHelp();
  static void PrintVersion();
};

#endif // PCBU_CLI_CLI_H
