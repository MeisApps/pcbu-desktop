#include "PairedDevicesStorage.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "AppSettings.h"
#include "shell/Shell.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <Windows.h>
#include <sddl.h>
#include <aclapi.h>
#elif APPLE
#define CHOWN_USER "root"
#elif LINUX
#define CHOWN_USER "root:root"
#endif

std::optional<PairedDevice> PairedDevicesStorage::GetDeviceByID(const std::string& pairingId) {
    for(const auto& device : GetDevices())
        if(device.pairingId == pairingId)
            return device;
    return {};
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevicesForUser(const std::string &userName) {
    std::vector<PairedDevice> result{};
    for(const auto& device : GetDevices()) {
        #ifdef WINDOWS
        if(StringUtils::ToLower(device.userName) == StringUtils::ToLower(userName))
            result.emplace_back(device);
        #else
        if(device.userName == userName)
            result.emplace_back(device);
        #endif
    }
    return result;
}

void PairedDevicesStorage::AddDevice(const PairedDevice &device) {
    auto devices = PairedDevicesStorage::GetDevices();
    auto rmRange = std::ranges::remove_if(devices, [&](const PairedDevice& d) {
        return d.pairingId == device.pairingId;
    });
    devices.erase(rmRange.begin(), rmRange.end());
    devices.emplace_back(device);
    PairedDevicesStorage::SaveDevices(devices);
}

void PairedDevicesStorage::RemoveDevice(const std::string &pairingId) {
    auto devices = PairedDevicesStorage::GetDevices();
    auto rmRange = std::ranges::remove_if(devices, [&](const PairedDevice& d) {
        return d.pairingId == pairingId;
    });
    devices.erase(rmRange.begin(), rmRange.end());
    PairedDevicesStorage::SaveDevices(devices);
}

std::vector<PairedDevice> PairedDevicesStorage::GetDevices() {
    std::vector<PairedDevice> result{};
    try {
        auto filePath = AppSettings::GetBaseDir() / DEVICES_FILE_NAME;
#ifdef WINDOWS
        ProtectFile(filePath.string(), false);
#endif
        auto jsonData = Shell::ReadBytes(filePath);
#ifdef WINDOWS
        ProtectFile(filePath.string(), true);
#endif
        auto json = nlohmann::json::parse(jsonData);
        for(auto entry : json) {
            auto device = PairedDevice();
            device.pairingId = entry["pairingId"];
            device.pairingMethod = PairingMethodUtils::FromString(entry["pairingMethod"]);
            device.deviceName = entry["deviceName"];
            device.userName = entry["userName"];
            device.encryptionKey = entry["encryptionKey"];

            device.ipAddress = entry["ipAddress"];
            device.bluetoothAddress = entry["bluetoothAddress"];
            device.cloudToken = entry["cloudToken"];
            result.emplace_back(device);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Failed reading paired devices storage: {}", ex.what());
        spdlog::info("Creating new devices storage...");
        SaveDevices({});
    }
    return result;
}

void PairedDevicesStorage::SaveDevices(const std::vector<PairedDevice> &devices) {
    try {
        nlohmann::json devicesJson{};
        for(auto device : devices) {
            nlohmann::json deviceJson = {
                    {"pairingId", device.pairingId},
                    {"pairingMethod", PairingMethodUtils::ToString(device.pairingMethod)},
                    {"deviceName", device.deviceName},
                    {"userName", device.userName},
                    {"encryptionKey", device.encryptionKey},

                    {"ipAddress", device.ipAddress},
                    {"bluetoothAddress", device.bluetoothAddress},
                    {"cloudToken", device.cloudToken}};
            devicesJson.emplace_back(deviceJson);
        }
        auto baseDir = AppSettings::GetBaseDir();
        if(!std::filesystem::exists(baseDir))
            Shell::CreateDir(baseDir);
        auto filePath = baseDir / DEVICES_FILE_NAME;
        ProtectFile(filePath.string(), false);
        auto jsonStr = devicesJson.dump();
        Shell::WriteBytes(filePath, {jsonStr.begin(), jsonStr.end()});
        ProtectFile(filePath.string(), true);
    } catch (const std::exception& ex) {
        spdlog::error("Failed writing paired devices storage: {}", ex.what());
    }
}

void PairedDevicesStorage::ProtectFile(const std::string &filePath, bool protect) {
    if(!std::filesystem::exists(filePath))
        return;
#ifdef WINDOWS
    PSID pSystemSid = nullptr;
    BOOL bIsSystem = FALSE;
    if (!ConvertStringSidToSidW(L"S-1-5-18", &pSystemSid)) {
        spdlog::error("Failed to create SYSTEM SID. (Code={})", GetLastError());
    } else if (!CheckTokenMembership(nullptr, pSystemSid, &bIsSystem)) {
        spdlog::error("Failed to check token membership. (Code={})", GetLastError());
        bIsSystem = FALSE;
    }
    if(pSystemSid) LocalFree(pSystemSid);
    if(!ModifyFileAccess(filePath, "S-1-5-32-545", protect)) {
        auto errorStr = "Error setting file permissions.";
        if(bIsSystem)
            spdlog::error(errorStr);
        else
            throw std::runtime_error(errorStr);
    }
#elif defined(APPLE) || defined(LINUX)
    if(protect) {
        auto cmd = Shell::RunCommand(fmt::format(R"(chown {0} "{1}" && chmod 600 "{1}")", CHOWN_USER, filePath));
        if(cmd.exitCode != 0)
            throw std::runtime_error(fmt::format("Error setting file permissions. (Code={})", cmd.exitCode));
    }
#endif
}

#ifdef WINDOWS
bool PairedDevicesStorage::ModifyFileAccess(const std::string& filePath, const std::string& sid, bool deny) {
    PSID pSid{};
    if (!ConvertStringSidToSidW(StringUtils::ToWideString(sid).c_str(), &pSid)) {
        spdlog::error("Failed to convert SID. (Code={})", GetLastError());
        return false;
    }

    auto filePathStr = StringUtils::ToWideString(filePath);
    PACL pOldDACL{};
    PSECURITY_DESCRIPTOR pSD{};
    DWORD dwRes = GetNamedSecurityInfoW(filePathStr.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
            nullptr, nullptr, &pOldDACL, nullptr, &pSD);
    if (dwRes != ERROR_SUCCESS) {
        spdlog::error("GetNamedSecurityInfo() failed. (Code={})", dwRes);
        if (pSid) LocalFree(pSid);
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
    if (GetAclInformation(pOldDACL, &aclSizeInfo, sizeof(ACL_SIZE_INFORMATION), AclSizeInformation)) {
        for (DWORD i = 0; i < aclSizeInfo.AceCount; ++i) {
            LPVOID pAce;
            if (GetAce(pOldDACL, i, &pAce)) {
                auto aceHeader = (ACE_HEADER *)pAce;
                bool isInherited = (aceHeader->AceFlags & INHERITED_ACE) != 0;
                if (!isInherited && (aceHeader->AceType == ACCESS_ALLOWED_ACE_TYPE || aceHeader->AceType == ACCESS_DENIED_ACE_TYPE)) {
                    auto ace = (ACCESS_ALLOWED_ACE *)pAce;
                    if (EqualSid(pSid, &ace->SidStart)) {
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
    if (!aceExists) {
        dwRes = SetEntriesInAclW(1, &ea, pOldDACL, &pNewDACL);
        if (dwRes != ERROR_SUCCESS) {
            spdlog::error("SetEntriesInAcl() failed. (Code={})", dwRes);
            if (pSD) LocalFree(pSD);
            if (pSid) LocalFree(pSid);
            return false;
        }
    } else {
        pNewDACL = pOldDACL;
    }
    dwRes = SetNamedSecurityInfoW((LPWSTR)filePathStr.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  nullptr, nullptr, pNewDACL, nullptr);
    if (dwRes != ERROR_SUCCESS) {
        spdlog::error("SetNamedSecurityInfo() failed. (Code={})", dwRes);
        if (!aceExists && pNewDACL) LocalFree(pNewDACL);
        if (pSD) LocalFree(pSD);
        if (pSid) LocalFree(pSid);
        return false;
    }

    if (!aceExists && pNewDACL) LocalFree(pNewDACL);
    if (pSD) LocalFree(pSD);
    if (pSid) LocalFree(pSid);
    return true;
}
#endif
