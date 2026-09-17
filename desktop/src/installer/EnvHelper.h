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
};

#endif // PCBU_DESKTOP_ENVHELPER_H
