#include "CommandLine.h"

#include <spdlog/spdlog.h>
#include <stdexcept>

CommandLine::CommandLine(const std::vector<std::string> &args, const std::set<std::string> &flags, const std::set<std::string> &valueOptions) {
  auto onlyArgs = false;
  for(size_t i = 0; i < args.size(); i++) {
    const auto &arg = args[i];
    if(onlyArgs || arg.size() < 2 || arg[0] != '-') {
      m_Args.push_back(arg);
      continue;
    }
    if(arg == "--") {
      onlyArgs = true;
      continue;
    }

    auto name = arg;
    std::optional<std::string> value{};
    if(auto separator = arg.find('='); separator != std::string::npos) {
      name = arg.substr(0, separator);
      value = arg.substr(separator + 1);
    }
    if(flags.contains(name)) {
      if(value.has_value())
        throw std::invalid_argument(fmt::format("Option '{}' does not take a value.", name));
      m_Flags.insert(name);
    } else if(valueOptions.contains(name)) {
      if(!value.has_value()) {
        if(i + 1 >= args.size())
          throw std::invalid_argument(fmt::format("Option '{}' requires a value.", name));
        value = args[++i];
      }
      m_Options[name] = value.value();
    } else {
      throw std::invalid_argument(fmt::format("Unknown option '{}'.", name));
    }
  }
}

void CommandLine::ExpectArgs(size_t minCount, size_t maxCount) const {
  if(m_Args.size() < minCount)
    throw std::invalid_argument("Missing argument.");
  if(m_Args.size() > maxCount)
    throw std::invalid_argument(fmt::format("Unexpected argument '{}'.", m_Args[maxCount]));
}

size_t CommandLine::GetArgCount() const {
  return m_Args.size();
}

std::string CommandLine::GetArg(size_t index) const {
  if(index >= m_Args.size())
    return {};
  return m_Args[index];
}

bool CommandLine::HasFlag(const std::string &name) const {
  return m_Flags.contains(name);
}

std::optional<std::string> CommandLine::GetOption(const std::string &name) const {
  if(auto it = m_Options.find(name); it != m_Options.end())
    return it->second;
  return {};
}
