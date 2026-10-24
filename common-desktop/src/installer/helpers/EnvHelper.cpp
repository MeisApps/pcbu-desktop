#include "EnvHelper.h"

#include "platform/PlatformHelper.h"
#include "shell/LocalShell.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"
#include <filesystem>

#define SSH_ENV_LINE_LINUX "SSH_ASKPASS=/usr/local/sbin/pcbu_ssh_askpass\nSSH_ASKPASS_REQUIRE=force"
#define SSH_ENV_LINE_MAC "export SSH_ASKPASS=/usr/local/sbin/pcbu_ssh_askpass\nexport SSH_ASKPASS_REQUIRE=force"

bool EnvHelper::IsSshEnabled() {
  auto path = GetEnvFile();
  if(!std::filesystem::exists(path))
    return false;
  auto fileData = LocalShell::ReadBytes(path);
  auto fileStr = std::string(fileData.begin(), fileData.end());
#ifdef APPLE
  return fileStr.contains(SSH_ENV_LINE_MAC);
#else
  return fileStr.contains(SSH_ENV_LINE_LINUX);
#endif
}

void EnvHelper::SetSshEnabled(bool enabled) {
  auto userInfo = GetUserInfo();
  auto homeDir = PlatformHelper::GetUserHomeDir(PlatformHelper::GetCurrentUser());
  if(!userInfo || homeDir.empty() || !std::filesystem::exists(homeDir))
    throw std::runtime_error(I18n::Get("error_file_write", "SSH-Env"));
  auto uid = userInfo->pw_uid;
  auto gid = userInfo->pw_gid;

  auto path = GetEnvFile();
  if(HasSymlinkBelow(homeDir, path))
    throw std::runtime_error(I18n::Get("error_file_write", path.string()));

  std::string fileStr{};
  if(std::filesystem::exists(path)) {
    auto fileData = LocalShell::ReadBytes(path);
    fileStr = std::string(fileData.begin(), fileData.end());
  }

  auto envLine = SSH_ENV_LINE_LINUX;
#ifdef APPLE
  envLine = SSH_ENV_LINE_MAC;
#endif
  auto hasLine = fileStr.contains(envLine);
  if(enabled == hasLine)
    return;
  if(enabled) {
    if(!fileStr.empty() && !fileStr.ends_with('\n'))
      fileStr += '\n';
    fileStr += envLine;
    fileStr += '\n';
  } else {
    fileStr = StringUtils::Replace(fileStr, envLine, "");
    if(fileStr.ends_with('\n'))
      fileStr.pop_back();
  }

  CreateUserDirs(homeDir, path.parent_path(), uid, gid);
  if(!LocalShell::WriteBytes(path, {fileStr.begin(), fileStr.end()}))
    throw std::runtime_error(I18n::Get("error_file_write", path.string()));
  SetUserOwner(path, uid, gid);
  if(LocalShell::RunCommand(fmt::format(R"(chmod 644 "{0}")", path.string())).exitCode != 0)
    throw std::runtime_error(fmt::format("Error setting file permissions."));
}

passwd *EnvHelper::GetUserInfo() {
  auto user = PlatformHelper::GetCurrentUser();
  auto pw = getpwnam(user.c_str());
  if(!pw)
    return {};
  return pw;
}

std::filesystem::path EnvHelper::GetEnvFile() {
  auto homeDir = PlatformHelper::GetUserHomeDir(PlatformHelper::GetCurrentUser());
  if(homeDir.empty())
    return {};
#ifdef APPLE
  return homeDir / ".zshenv";
#else
  return homeDir / ".config/environment.d/pulseunlock-ssh.conf";
#endif
}

bool EnvHelper::HasSymlinkBelow(const std::filesystem::path &homeDir, const std::filesystem::path &path) {
  auto currPath = homeDir;
  for(const auto &part : path.lexically_relative(homeDir)) {
    currPath /= part;
    if(std::filesystem::is_symlink(currPath))
      return true;
  }
  return false;
}

void EnvHelper::CreateUserDirs(const std::filesystem::path &homeDir, const std::filesystem::path &dir, uid_t uid, gid_t gid) {
  auto currDir = homeDir;
  for(const auto &part : dir.lexically_relative(homeDir)) {
    currDir /= part;
    if(std::filesystem::exists(currDir))
      continue;
    if(!LocalShell::CreateDir(currDir))
      throw std::runtime_error(I18n::Get("error_file_write", currDir.string()));
    SetUserOwner(currDir, uid, gid);
  }
}

void EnvHelper::SetUserOwner(const std::filesystem::path &path, uid_t uid, gid_t gid) {
  if(!LocalShell::IsRunningAsAdmin())
    return;
  if(LocalShell::RunCommand(fmt::format(R"(chown {0}:{1} "{2}")", uid, gid, path.string())).exitCode != 0)
    throw std::runtime_error(fmt::format("Error setting file permissions."));
}
