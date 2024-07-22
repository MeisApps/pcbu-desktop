#include "NetworkHelper.h"

#include <algorithm>
#include <iostream>
#include <regex>
#include <map>
#include <spdlog/spdlog.h>

#include "storage/AppSettings.h"
#include "utils/StringUtils.h"

#ifdef WINDOWS
#include <WinSock2.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <SensAPI.h>
#include <Netlistmgr.h>
#elif defined(LINUX) || defined(APPLE)
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#ifdef LINUX
#include <netpacket/packet.h>
#elif APPLE
#define AF_PACKET AF_LINK
#endif
#endif

std::regex g_LocalIPv4Regex(R"((^10\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$)|(^172\.1[6-9]{1}[0-9]{0,1}\.[0-9]{1,3}\.[0-9]{1,3}$)|(^172\.2[0-9]{1}[0-9]{0,1}\.[0-9]{1,3}\.[0-9]{1,3}$)|(^172\.3[0-1]{1}[0-9]{0,1}\.[0-9]{1,3}\.[0-9]{1,3}$)|(^192\.168\.[0-9]{1,3}\.[0-9]{1,3}$))");

std::vector<NetworkInterface> NetworkHelper::GetLocalNetInterfaces() {
    std::vector<NetworkInterface> result{};
#ifdef WINDOWS
    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
    std::vector<BYTE> buffer(bufferSize);
    auto adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterAddresses, &bufferSize))
        return {};

    for (PIP_ADAPTER_ADDRESSES adapter = adapterAddresses; adapter; adapter = adapter->Next) {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->OperStatus != IfOperStatusUp)
            continue;

        char macBuffer[18]{};
        snprintf(macBuffer, sizeof(macBuffer),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 adapter->PhysicalAddress[0], adapter->PhysicalAddress[1],
                 adapter->PhysicalAddress[2], adapter->PhysicalAddress[3],
                 adapter->PhysicalAddress[4], adapter->PhysicalAddress[5]);
        auto ifName = std::wstring(adapter->FriendlyName);
        auto macAddr = std::string(macBuffer);

        auto netIf = NetworkInterface();
        netIf.ifName = StringUtils::FromWideString(ifName);
        netIf.macAddress = macAddr;
        for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            auto family = unicast->Address.lpSockaddr->sa_family;
            if (family != AF_INET)
                continue;
            auto ipv4 = reinterpret_cast<SOCKADDR_IN *>(unicast->Address.lpSockaddr);
            char strBuffer[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &(ipv4->sin_addr), strBuffer, INET_ADDRSTRLEN);
            auto ifAddr = std::string(strBuffer);
            netIf.ipAddress = ifAddr;
        }
        if(!netIf.ipAddress.empty())
            result.emplace_back(netIf);
    }
#elif defined(LINUX) || defined(APPLE)
    struct ifaddrs *ifaddr{};
    if(getifaddrs(&ifaddr))
        return {};

    std::map<std::string, NetworkInterface> ifMap{};
    for(auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if(ifa->ifa_addr == nullptr || ifa->ifa_flags & IFF_LOOPBACK || !(ifa->ifa_flags & IFF_UP))
            continue;
        int family = ifa->ifa_addr->sa_family;
        if(family == AF_INET || family == AF_PACKET) {
            char addr[NI_MAXHOST]{};
            auto res = getnameinfo(ifa->ifa_addr, (family == AF_INET) ?
                    sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                    addr, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if(res && family != AF_PACKET) {
                printf("getnameinfo() failed: %s\n", gai_strerror(res));
                continue;
            }

            auto ifName = ifa->ifa_name;
            if(!ifMap.contains(ifName)) {
                ifMap[ifName] = NetworkInterface();
                ifMap[ifName].ifName = ifName;
            }
            if(family == AF_INET)
                ifMap[ifName].ipAddress = addr;
            if(family == AF_PACKET) {
#ifdef LINUX
                auto sll = reinterpret_cast<struct sockaddr_ll*>(ifa->ifa_addr);
                snprintf(addr, sizeof(addr),
                         "%02X:%02X:%02X:%02X:%02X:%02X",
                         sll->sll_addr[0], sll->sll_addr[1],
                         sll->sll_addr[2], sll->sll_addr[3],
                         sll->sll_addr[4], sll->sll_addr[5]);
#endif
                ifMap[ifName].macAddress = addr;
            }

        }
    }

    for(const auto& pair : ifMap)
        if(!pair.second.ipAddress.empty())
            result.emplace_back(pair.second);
    freeifaddrs(ifaddr);
#endif

    auto rankIp = [](const NetworkInterface& netIf){
        auto rank = 0;
        if(std::regex_match(netIf.ipAddress, g_LocalIPv4Regex))
            rank += 1000;
        if(netIf.ipAddress.starts_with("192.168.") || netIf.ipAddress.starts_with("10."))
            rank += 50;
#ifdef WINDOWS
        if(netIf.ifName.contains("Bluetooth")
        || netIf.ifName.contains("vEthernet")
        || netIf.ifName.contains("VMware")
        || netIf.ifName.contains("VirtualBox")
        || netIf.ifName.contains("Docker"))
            rank -= 100;
#else
        if(netIf.ifName.starts_with("vir")
        || netIf.ifName.starts_with("docker")
        || netIf.ifName.starts_with("ham")
        || netIf.ifName.starts_with("bridge"))
            rank -= 100;
#endif
        return rank;
    };
    std::sort(result.begin(), result.end(), [rankIp](const NetworkInterface& a, const NetworkInterface& b) {
        auto rankA = rankIp(a);
        auto rankB = rankIp(b);
        if(rankA == rankB)
            return StringUtils::ToLower(a.ifName) < StringUtils::ToLower(b.ifName);
        return rankA > rankB;
    });
    return result;
}

std::string NetworkHelper::GetHostName() {
#ifdef WINDOWS
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName) / sizeof(computerName[0]);
    if (GetComputerNameA(computerName, &size))
        return {computerName};
    spdlog::error("Failed to get computer name.");
    return {};
#elif defined(LINUX) || defined(APPLE)
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0)
        return {hostname};
    spdlog::error("Failed to get computer name.");
    return {};
#endif
}

bool NetworkHelper::HasLANConnection() {
#ifdef WINDOWS
    DWORD flags{};
    auto hasLAN = IsNetworkAlive(&flags) && GetLastError() == 0 && (flags & NETWORK_ALIVE_LAN) == NETWORK_ALIVE_LAN;

    HRESULT hr;
    INetworkListManager *pNetworkListManager = nullptr;
    NLM_CONNECTIVITY nlmConnectivity = NLM_CONNECTIVITY_DISCONNECTED;
    hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        spdlog::error("Failed to initialize COM. (Code={})", hr);
        return hasLAN;
    }
    hr = CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_INetworkListManager, (void **)&pNetworkListManager);
    if (FAILED(hr)) {
        spdlog::error("Failed to create INetworkListManager. (Code={})", hr);
    } else {
        hr = pNetworkListManager->GetConnectivity(&nlmConnectivity);
        if (FAILED(hr)) {
            spdlog::error("Failed to get network connectivity. (Code={})", hr);
        } else {
            hasLAN = nlmConnectivity & NLM_CONNECTIVITY_IPV4_SUBNET
                    || nlmConnectivity & NLM_CONNECTIVITY_IPV4_LOCALNETWORK
                    || nlmConnectivity & NLM_CONNECTIVITY_IPV4_INTERNET;
        }
        pNetworkListManager->Release();
    }
    CoUninitialize();
    return hasLAN;
#endif
    return false;
}

NetworkInterface NetworkHelper::GetSavedNetworkInterface() {
    NetworkInterface result{};
    auto localIfs = NetworkHelper::GetLocalNetInterfaces();
    auto settings = AppSettings::Get();
    auto found = false;
    if(settings.serverIP == "auto") {
        for(const auto& netIf : localIfs) {
            if(netIf.macAddress == settings.serverMAC) {
                result = netIf;
                found = true;
                break;
            }
        }
        if(!found) {
            if(!localIfs.empty())
                result = localIfs[0];
            else
                spdlog::warn("Invalid server IP settings.");
        }
    } else {
        result.ipAddress = settings.serverIP;
    }
    return result;
}
