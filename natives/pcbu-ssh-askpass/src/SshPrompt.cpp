#include "SshPrompt.h"

#include <ranges>
#include <string_view>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "utils/StringUtils.h"

static std::string CanonicalizePath(const std::string &path, const std::string &homeDir) {
  namespace fs = boost::filesystem;
  auto expanded = path;
  if(expanded == "~")
    expanded = homeDir;
  else if(expanded.starts_with("~/"))
    expanded = (fs::path(homeDir) / expanded.substr(2)).string();

  boost::system::error_code ec{};
  auto absolute = fs::absolute(expanded, ec);
  if(ec)
    return expanded;
  auto canonical = fs::weakly_canonical(absolute, ec);
  if(ec)
    return absolute.lexically_normal().string();
  return canonical.string();
}

SshPrompt SshPrompt::Parse(const std::string &raw) {
  auto prompt = raw;
  boost::algorithm::trim(prompt);

  constexpr std::string_view passwordSuffix = "'s password:";
  if(boost::algorithm::iends_with(prompt, passwordSuffix)) {
    auto ident = prompt.substr(0, prompt.size() - passwordSuffix.size());
    auto result = SshPrompt();
    result.kind = Kind::Password;
    if(auto at = ident.rfind('@'); at != std::string::npos) {
      result.user = ident.substr(0, at);
      result.host = ident.substr(at + 1);
    } else {
      result.host = ident;
    }
    return result;
  }

  if(!boost::algorithm::istarts_with(prompt, "enter passphrase for"))
    return {};

  auto result = SshPrompt();
  result.kind = Kind::Key;
  if(auto start = prompt.find('\''); start != std::string::npos) {
    if(auto end = prompt.find('\'', start + 1); end != std::string::npos)
      result.keyPath = prompt.substr(start + 1, end - start - 1);
  }
  if(result.keyPath.empty()) {
    if(boost::algorithm::ends_with(prompt, ":"))
      prompt.pop_back();
    auto space = prompt.rfind(' ');
    auto token = space == std::string::npos ? prompt : prompt.substr(space + 1);
    if(token.starts_with('/') || token.starts_with('~') || token.starts_with('.'))
      result.keyPath = token;
  }
  if(result.keyPath.empty())
    return {};
  return result;
}

std::string SshPrompt::GetDisplayName() const {
  constexpr uint32_t maxTargetLen = 128;
  auto target = std::string();
  if(kind == Kind::Password)
    target = user.empty() ? host : user + "@" + host;
  else if(kind == Kind::Key)
    target = keyPath;
  std::erase_if(target, [](unsigned char c) { return c < 0x20 || c == 0x7f; });
  target = StringUtils::Trim(target);
  if(target.empty())
    return "ssh";
  return "ssh " + StringUtils::Truncate(target, maxTargetLen);
}

std::optional<SshServer> SshPrompt::FindMatch(const PairedDevice &device, const std::string &homeDir) const {
  auto matches = [&](const SshServer &server) {
    if(kind == Kind::Password) {
      if(server.host.empty() || host.empty())
        return false;
      if(!boost::algorithm::iequals(server.host, host))
        return false;
      return server.user.empty() || server.user == user;
    }
    if(kind == Kind::Key) {
      if(server.keyPath.empty() || keyPath.empty())
        return false;
      return CanonicalizePath(server.keyPath, homeDir) == CanonicalizePath(keyPath, homeDir);
    }
    return false;
  };

  if(kind == Kind::Password) {
    auto specific = std::ranges::find_if(device.sshServers, [&](const SshServer &server) { return matches(server) && !server.user.empty(); });
    if(specific != device.sshServers.end())
      return *specific;
  }

  auto match = std::ranges::find_if(device.sshServers, matches);
  if(match == device.sshServers.end())
    return {};
  return *match;
}
