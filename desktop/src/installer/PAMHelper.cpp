#include "PAMHelper.h"

#include <spdlog/spdlog.h>

#include "shell/Shell.h"
#include "utils/I18n.h"
#include "utils/StringUtils.h"

#define PAM_CONFIG_DIR std::filesystem::path("/etc/pam.d/")

constexpr std::string_view PAM_CONFIG_GEN_ENTRY = "## Generated by PC Bio Unlock";
constexpr std::string_view PAM_CONFIG_GEN_COMMON = R"(
#%PAM-1.0
{0}
{1}

auth include common-auth
account include common-account
password include common-password
session include common-session
)";
constexpr std::string_view PAM_CONFIG_GEN_SYSTEM = R"(
#%PAM-1.0
{0}
{1}

auth include system-auth
account include system-auth
password include system-auth
session include system-auth
)";

PAMHelper::PAMHelper(const std::function<void(const std::string &)> &logCallback) {
  m_Logger = logCallback;
}

bool PAMHelper::HasConfigEntry(const std::string &configName, const std::string &entry) {
  auto configPath = PAM_CONFIG_DIR / configName;
  if(std::filesystem::exists(configPath)) {
    auto fileData = Shell::ReadBytes(configPath);
    auto fileStr = std::string(fileData.begin(), fileData.end());
    return fileStr.contains(entry);
  }
  return false;
}

void PAMHelper::SetConfigEntry(const std::string &configName, const std::string &entry, bool enabled) {
  auto hasEntry = HasConfigEntry(configName, entry);
  if(enabled && !hasEntry)
    AddToConfig(configName, entry);
  else if(!enabled && hasEntry)
    RemoveFromConfig(configName, entry);
}

bool PAMHelper::IsConfigGenerated(const std::filesystem::path &filePath) {
  auto fileData = Shell::ReadBytes(filePath);
  auto fileStr = std::string(fileData.begin(), fileData.end());
  return fileStr.contains(PAM_CONFIG_GEN_ENTRY);
}

void PAMHelper::AddToConfig(const std::string &configName, const std::string &entry) {
  auto configPath = PAM_CONFIG_DIR / configName;
  if(!std::filesystem::exists(configPath)) {
    auto hasCommonAuth = std::filesystem::exists(PAM_CONFIG_DIR / "common-auth");
    auto hasSystemAuth = std::filesystem::exists(PAM_CONFIG_DIR / "system-auth");

    m_Logger(fmt::format("Generating configuration {}...", configName));
    std::string fileStr{};
    if(hasSystemAuth)
      fileStr = fmt::format(PAM_CONFIG_GEN_SYSTEM, entry, PAM_CONFIG_GEN_ENTRY);
    else
      fileStr = fmt::format(PAM_CONFIG_GEN_COMMON, entry, PAM_CONFIG_GEN_ENTRY);
    if(!Shell::WriteBytes(configPath, {fileStr.begin(), fileStr.end()}))
      throw std::runtime_error(I18n::Get("error_file_write", configPath.string()));

    if(!hasCommonAuth && !hasSystemAuth)
      m_Logger("Unknown PAM configuration. Please report on GitHub.");
  } else {
    AddEntryToFile(configPath, entry);
  }
}

void PAMHelper::RemoveFromConfig(const std::string &configName, const std::string &entry) {
  auto configPath = PAM_CONFIG_DIR / configName;
  if(!std::filesystem::exists(configPath))
    return;

  if(IsConfigGenerated(configPath)) {
    m_Logger(fmt::format("Removing generated configuration {}...", configName));
    if(!Shell::RemoveFile(configPath))
      throw std::runtime_error(I18n::Get("error_file_remove", configPath.string()));
  } else {
    RemoveEntryFromFile(configPath, entry);
  }
}

void PAMHelper::AddEntryToFile(const std::filesystem::path &filePath, const std::string &entry) {
  auto fileData = Shell::ReadBytes(filePath);
  auto fileStr = std::string(fileData.begin(), fileData.end());

  m_Logger(fmt::format("Adding PAM entry to {}...", filePath.string()));
  std::string resultStr{};
  auto hasBegin = fileStr.contains("#%PAM-1.0\n");
  if(!hasBegin) {
    resultStr.append("#%PAM-1.0\n");
    resultStr.append(entry + '\n');
  }

  for(const auto &line : StringUtils::Split(fileStr, "\n")) {
    if(line == "#%PAM-1.0" && hasBegin) {
      resultStr.append(line + '\n');
      resultStr.append(entry + '\n');
    } else {
      resultStr.append(line + '\n');
    }
  }
  if(!Shell::WriteBytes(filePath, {resultStr.begin(), resultStr.end()}))
    throw std::runtime_error(I18n::Get("error_file_write", filePath.string()));
}

void PAMHelper::RemoveEntryFromFile(const std::filesystem::path &filePath, const std::string &entry) {
  auto fileData = Shell::ReadBytes(filePath);
  auto fileStr = std::string(fileData.begin(), fileData.end());

  m_Logger(fmt::format("Removing PAM entry from {}...", filePath.string()));
  std::string resultStr{};
  for(const auto &line : StringUtils::Split(fileStr, "\n"))
    if(line != entry)
      resultStr.append(line + '\n');
  if(!Shell::WriteBytes(filePath, {resultStr.begin(), resultStr.end()}))
    throw std::runtime_error(I18n::Get("error_file_write", filePath.string()));
}
