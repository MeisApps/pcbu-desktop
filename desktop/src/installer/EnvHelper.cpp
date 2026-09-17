#include "EnvHelper.h"

#include "platform/PlatformHelper.h"
#include "shell/Shell.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"
#include <filesystem>

#define SSH_ENV_LINE_LINUX "SSH_ASKPASS=/usr/local/sbin/pcbu_ssh_askpass\nSSH_ASKPASS_REQUIRE=force"
#define SSH_ENV_LINE_MAC "export SSH_ASKPASS=/usr/local/sbin/pcbu_ssh_askpass\nexport SSH_ASKPASS_REQUIRE=force"

bool EnvHelper::IsSshEnabled() {
  auto path = GetEnvFile();
  if(!std::filesystem::exists(path))
    return false;
  auto fileData = Shell::ReadBytes(path);
  auto fileStr = std::string(fileData.begin(), fileData.end());
#ifdef APPLE
  return fileStr.contains(SSH_ENV_LINE_MAC);
#else
  return fileStr.contains(SSH_ENV_LINE_LINUX);
#endif
}

void EnvHelper::SetSshEnabled(bool enabled) {
  auto userInfo = GetUserInfo();
  if(!userInfo || !std::filesystem::exists(userInfo->pw_dir))
    throw std::runtime_error(I18n::Get("error_file_write", "SSH-Env"));

  auto path = GetEnvFile();
  if(std::filesystem::is_symlink(path))
    throw std::runtime_error(I18n::Get("error_file_write", path.string()));

#ifdef LINUX // ToDo: Remove this once elevator ships
  if(enabled) {
    auto configDir = std::filesystem::path(userInfo->pw_dir) / ".config/";
    auto envDir = std::filesystem::path(userInfo->pw_dir) / ".config/environment.d/";
    if(!std::filesystem::exists(configDir)) {
      Shell::CreateDir(configDir);
      if(Shell::RunCommand(fmt::format(R"(chown {0}:{1} "{2}")", userInfo->pw_uid, userInfo->pw_gid, configDir.string())).exitCode != 0)
        throw std::runtime_error(fmt::format("Error setting file permissions."));
    }
    if(!std::filesystem::exists(envDir)) {
      Shell::CreateDir(envDir);
      if(Shell::RunCommand(fmt::format(R"(chown {0}:{1} "{2}")", userInfo->pw_uid, userInfo->pw_gid, envDir.string())).exitCode != 0)
        throw std::runtime_error(fmt::format("Error setting file permissions."));
    }
  }
#endif

  std::string fileStr{};
  if(std::filesystem::exists(path)) {
    auto fileData = Shell::ReadBytes(path);
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

  if(!Shell::WriteBytes(path, {fileStr.begin(), fileStr.end()}))
    throw std::runtime_error(I18n::Get("error_file_write", path.string()));
  if(Shell::RunCommand(fmt::format(R"(chown {0}:{1} "{2}" && chmod 644 "{2}")", userInfo->pw_uid, userInfo->pw_gid, path.string())).exitCode != 0)
    throw std::runtime_error(fmt::format("Error setting file permissions."));
}

passwd *EnvHelper::GetUserInfo() {
  auto user = PlatformHelper::GetCurrentUser();
  auto pw = getpwnam(user.c_str());
  if(!pw || !pw->pw_dir)
    return {};
  return pw;
}

std::filesystem::path EnvHelper::GetEnvFile() {
  auto userInfo = GetUserInfo();
  if(!userInfo)
    return {};
#ifdef APPLE
  return std::filesystem::path(userInfo->pw_dir) / ".zshenv";
#endif
  return std::filesystem::path(userInfo->pw_dir) / ".config/environment.d/pulseunlock-ssh.conf";
}
