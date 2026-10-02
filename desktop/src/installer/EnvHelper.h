#ifndef PCBU_DESKTOP_ENVHELPER_H
#define PCBU_DESKTOP_ENVHELPER_H

#include <filesystem>

#include <pwd.h>

class EnvHelper {
public:
  static bool IsSshEnabled();
  static void SetSshEnabled(bool enabled);

private:
  EnvHelper() = default;

  static passwd *GetUserInfo();
  static std::filesystem::path GetEnvFile();

  static bool HasSymlinkBelow(const std::filesystem::path &homeDir, const std::filesystem::path &path);
  static void CreateUserDirs(const std::filesystem::path &homeDir, const std::filesystem::path &dir, uid_t uid, gid_t gid);
  static void SetUserOwner(const std::filesystem::path &path, uid_t uid, gid_t gid);
};

#endif // PCBU_DESKTOP_ENVHELPER_H
