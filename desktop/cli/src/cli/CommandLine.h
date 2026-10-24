#ifndef PCBU_CLI_COMMANDLINE_H
#define PCBU_CLI_COMMANDLINE_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class CommandLine {
public:
  explicit CommandLine(const std::vector<std::string> &args, const std::set<std::string> &flags = {}, const std::set<std::string> &valueOptions = {});

  void ExpectArgs(size_t minCount, size_t maxCount) const;
  [[nodiscard]] size_t GetArgCount() const;
  [[nodiscard]] std::string GetArg(size_t index) const;

  [[nodiscard]] bool HasFlag(const std::string &name) const;
  [[nodiscard]] std::optional<std::string> GetOption(const std::string &name) const;

private:
  std::vector<std::string> m_Args{};
  std::set<std::string> m_Flags{};
  std::map<std::string, std::string> m_Options{};
};

#endif // PCBU_CLI_COMMANDLINE_H
