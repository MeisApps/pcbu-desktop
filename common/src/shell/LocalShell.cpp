#include "LocalShell.h"

#include <boost/filesystem.hpp>
#include <boost/process/v1.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Windows.h>
#include <aclapi.h>
#include <boost/process/v1/windows.hpp>
#include <sddl.h>

#define SHELL_NAME "cmd.exe"
#define SHELL_CMD_ARG "/c"
#elif LINUX
#define SHELL_NAME "bash"
#define SHELL_CMD_ARG "-c"
#define CHOWN_USER "root:root"
#elif APPLE
#define SHELL_NAME "zsh"
#define SHELL_CMD_ARG "-c"
#define CHOWN_USER "root"
#endif

bool LocalShell::IsRunningAsAdmin() {
#ifdef WINDOWS
  BOOL isAdmin = FALSE;
  PSID adminGroup = nullptr;
  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
  if(!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
    spdlog::error("AllocateAndInitializeSid failed. (Code={})", GetLastError());
    return false;
  }
  if(!CheckTokenMembership(nullptr, adminGroup, &isAdmin)) {
    spdlog::error("CheckTokenMembership failed. (Code={})", GetLastError());
    isAdmin = FALSE;
  }
  if(adminGroup)
    FreeSid(adminGroup);
  return isAdmin;
#else
  return geteuid() == 0;
#endif
}

ShellCmdResult LocalShell::RunCommand(const std::string &cmd) {
  return RunUserCommand(cmd);
}

ShellCmdResult LocalShell::RunUserCommand(const std::string &cmd) {
  boost::process::v1::ipstream outStream{};
  boost::process::v1::ipstream errStream{};
#ifdef WINDOWS
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd), boost::process::v1::std_out > outStream,
                                 boost::process::v1::std_err > errStream, boost::process::v1::windows::create_no_window);
#else
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd), boost::process::v1::std_out > outStream,
                                 boost::process::v1::std_err > errStream);
#endif
  std::string output{};
  std::string line{};
  while(outStream && std::getline(outStream, line) && !line.empty())
    output.append(line + "\n");
  while(errStream && std::getline(errStream, line) && !line.empty())
    output.append(line + "\n");
  proc.wait();

  spdlog::debug("Process exit. Code: {} Command: '{}' Output: '{}'", proc.exit_code(), cmd, output);
  auto result = ShellCmdResult();
  result.exitCode = proc.exit_code();
  result.output = output;
  return result;
}

void LocalShell::SpawnCommand(const std::string &cmd) {
#ifdef WINDOWS
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd), boost::process::v1::windows::create_no_window);
#else
  boost::process::v1::child proc(fmt::format("{0} {1} \"{2}\"", SHELL_NAME, SHELL_CMD_ARG, cmd));
#endif
  proc.detach();
}

bool LocalShell::CreateDir(const std::filesystem::path &path) {
  boost::system::error_code ec{};
  boost::filesystem::create_directories(boost::filesystem::path(path), ec);
  return !ec.failed();
}

bool LocalShell::RemoveDir(const std::filesystem::path &path) {
  boost::system::error_code ec{};
  boost::filesystem::remove(boost::filesystem::path(path), ec);
  return !ec.failed();
}

bool LocalShell::RemoveFile(const std::filesystem::path &path) {
  boost::system::error_code ec{};
  boost::filesystem::remove(boost::filesystem::path(path), ec);
  return !ec.failed();
}

std::vector<uint8_t> LocalShell::ReadBytes(const std::filesystem::path &path) {
  std::ifstream file{};
  file.open(path, std::ios_base::binary);
  if(!file) {
    spdlog::error("Failed to open file '{}'.", path.string());
    return {};
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

bool LocalShell::WriteBytes(const std::filesystem::path &path, const std::vector<uint8_t> &data) {
  std::ofstream file(path, std::ios::out | std::ios::binary);
  file.write(reinterpret_cast<const char *>(data.data()), (std::streamsize)data.size());
  file.close();
  return !file.fail() && !file.bad();
}

bool LocalShell::ProtectFile(const std::filesystem::path &path, bool enabled) {
  if(!std::filesystem::exists(path))
    return false;
#ifdef WINDOWS
  PSID pSystemSid = nullptr;
  BOOL bIsSystem = FALSE;
  if(!ConvertStringSidToSidW(L"S-1-5-18", &pSystemSid)) {
    spdlog::error("Failed to create SYSTEM SID. (Code={})", GetLastError());
  } else if(!CheckTokenMembership(nullptr, pSystemSid, &bIsSystem)) {
    spdlog::error("Failed to check token membership. (Code={})", GetLastError());
    bIsSystem = FALSE;
  }
  if(pSystemSid)
    LocalFree(pSystemSid);
  if(!ModifyFileAccess(filePath, "S-1-5-32-545", enabled)) {
    spdlog::error("Error setting file permissions.");
    return bIsSystem;
  }
#else
  if(enabled) {
    auto cmd = RunCommand(fmt::format(R"(chown {0} "{1}" && chmod 600 "{1}")", CHOWN_USER, path.string()));
    if(cmd.exitCode != 0) {
      spdlog::error("Error setting file permissions. (Code={})", cmd.exitCode);
      return false;
    }
  }
#endif
  return true;
}

#ifdef WINDOWS
bool LocalShell::ModifyFileAccess(const std::string &filePath, const std::string &sid, bool deny) {
  PSID pSid{};
  if(!ConvertStringSidToSidW(StringUtils::ToWideString(sid).c_str(), &pSid)) {
    spdlog::error("Failed to convert SID. (Code={})", GetLastError());
    return false;
  }

  auto filePathStr = StringUtils::ToWideString(filePath);
  PACL pOldDACL{};
  PSECURITY_DESCRIPTOR pSD{};
  DWORD dwRes = GetNamedSecurityInfoW(filePathStr.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pOldDACL, nullptr, &pSD);
  if(dwRes != ERROR_SUCCESS) {
    spdlog::error("GetNamedSecurityInfo() failed. (Code={})", dwRes);
    if(pSid)
      LocalFree(pSid);
    return false;
  }

  EXPLICIT_ACCESS_W ea{};
  ea.grfAccessPermissions = GENERIC_ALL;
  ea.grfAccessMode = deny ? DENY_ACCESS : GRANT_ACCESS;
  ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  ea.Trustee.ptstrName = (LPWCH)pSid;
  bool aceExists = false;
  ACL_SIZE_INFORMATION aclSizeInfo{};
  if(GetAclInformation(pOldDACL, &aclSizeInfo, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation)) {
    for(DWORD i = 0; i < aclSizeInfo.AceCount; ++i) {
      LPVOID pAce;
      if(GetAce(pOldDACL, i, &pAce)) {
        auto aceHeader = (ACE_HEADER *)pAce;
        bool isInherited = (aceHeader->AceFlags & INHERITED_ACE) != 0;
        if(!isInherited && (aceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE || aceHeader->AceType == ACCESS_DENIED_ACE_TYPE)) {
          auto ace = (ACCESS_ALLOWED_ACE *)pAce;
          if(EqualSid(pSid, &ace->SidStart)) {
            aceExists = true;
            ace->Mask = ea.grfAccessPermissions;
            aceHeader->AceType = deny ? ACCESS_DENIED_ACE_TYPE : ACCESS_ALLOWED_ACE_TYPE;
            break;
          }
        }
      }
    }
  }

  PACL pNewDACL{};
  if(!aceExists) {
    dwRes = SetEntriesInAclW(1, &ea, pOldDACL, &pNewDACL);
    if(dwRes != ERROR_SUCCESS) {
      spdlog::error("SetEntriesInAcl() failed. (Code={})", dwRes);
      if(pSD)
        LocalFree(pSD);
      if(pSid)
        LocalFree(pSid);
      return false;
    }
  } else {
    pNewDACL = pOldDACL;
  }
  dwRes = SetNamedSecurityInfoW((LPWSTR)filePathStr.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewDACL, nullptr);
  if(dwRes != ERROR_SUCCESS) {
    spdlog::error("SetNamedSecurityInfo() failed. (Code={})", dwRes);
    if(!aceExists && pNewDACL)
      LocalFree(pNewDACL);
    if(pSD)
      LocalFree(pSD);
    if(pSid)
      LocalFree(pSid);
    return false;
  }

  if(!aceExists && pNewDACL)
    LocalFree(pNewDACL);
  if(pSD)
    LocalFree(pSD);
  if(pSid)
    LocalFree(pSid);
  return true;
}
#endif
