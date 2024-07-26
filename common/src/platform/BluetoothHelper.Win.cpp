#include "BluetoothHelper.h"

#include <spdlog/spdlog.h>

#include "utils/StringUtils.h"

bool BluetoothHelper::IsAvailable() {
    BLUETOOTH_FIND_RADIO_PARAMS params{};
    params.dwSize = sizeof(params);
    HANDLE hRadio = nullptr;
    HANDLE result = BluetoothFindFirstRadio(&params, &hRadio);
    if (result)
        CloseHandle(hRadio);
    return result;
}

void BluetoothHelper::StartScan() {
}

void BluetoothHelper::StopScan() {
}

std::vector<BluetoothDevice> BluetoothHelper::ScanDevices() {
    WSADATA data;
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0)
        return {};

    WSAQUERYSETW queryset{};
    queryset.dwSize = sizeof(WSAQUERYSETW);
    queryset.dwNameSpace = NS_BTH;
    HANDLE hLookup;
    result = WSALookupServiceBeginW(&queryset, LUP_RETURN_NAME | LUP_CONTAINERS | LUP_RETURN_ADDR | LUP_FLUSHCACHE | LUP_RETURN_TYPE | LUP_RETURN_BLOB | LUP_RES_SERVICE, &hLookup);
    if (result != 0)
        return {};

    BYTE buffer[4096]{};
    DWORD bufferLength = sizeof(buffer);
    auto pResults = reinterpret_cast<WSAQUERYSETW*>(&buffer);
    auto devices = std::vector<BluetoothDevice>();
    while (result == 0) {
        result = WSALookupServiceNextW(hLookup, LUP_RETURN_NAME | LUP_CONTAINERS | LUP_RETURN_ADDR | LUP_FLUSHCACHE | LUP_RETURN_TYPE | LUP_RETURN_BLOB | LUP_RES_SERVICE, &bufferLength, pResults);
        if (result == 0) {
            auto pBtAddr = reinterpret_cast<SOCKADDR_BTH*>(pResults->lpcsaBuffer->RemoteAddr.lpSockaddr);
            char addr[18]{};
            ba2str(pBtAddr->btAddr, addr);
            auto name = StringUtils::FromWideString(pResults->lpszServiceInstanceName);
            auto address = std::string(addr);
            if(name.empty())
                name = "Unknown device";
            devices.push_back({name, address});
        }
    }
    WSALookupServiceEnd(hLookup);
    return devices;
}

bool BluetoothHelper::PairDevice(const BluetoothDevice &device) {
    BLUETOOTH_DEVICE_INFO deviceInfo = { sizeof(deviceInfo) };
    BLUETOOTH_ADDRESS deviceAddress;
    sscanf_s(device.address.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &deviceAddress.rgBytes[5], &deviceAddress.rgBytes[4], &deviceAddress.rgBytes[3],
             &deviceAddress.rgBytes[2], &deviceAddress.rgBytes[1], &deviceAddress.rgBytes[0]);

    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {sizeof(searchParams), TRUE, FALSE,
            TRUE, TRUE, TRUE, 5, nullptr
    };
    HANDLE searchHandle = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    if (!searchHandle) {
        spdlog::error("Error getting bluetooth search handle.");
        return false;
    }
    bool deviceFound = false;
    do {
        char devAddr[18]{};
        char targetAddr[18]{};
        ba2str(deviceInfo.Address.ullLong, devAddr);
        ba2str(deviceAddress.ullLong, targetAddr);
        if(strcmp(devAddr, targetAddr) == 0) {
            deviceFound = true;
            break;
        }
    } while (BluetoothFindNextDevice(searchHandle, &deviceInfo));
    BluetoothFindDeviceClose(searchHandle);
    if (!deviceFound) {
        spdlog::error("Bluetooth device not found.");
        return false;
    }

    HWND hwnd = FindWindowA(nullptr, "PC Bio Unlock");
    DWORD result = BluetoothAuthenticateDevice(hwnd, nullptr, &deviceInfo, nullptr, 0);
    if (result == ERROR_SUCCESS || result == ERROR_NO_MORE_ITEMS)
        return true;
    spdlog::error("Error while bluetooth pairing. (Code={})", result);
    return false;
}

int BluetoothHelper::str2ba(const char *straddr, BTH_ADDR *btaddr) {
    unsigned int aaddr[6]{};
    if (sscanf_s(straddr, "%02x:%02x:%02x:%02x:%02x:%02x",
                 &aaddr[0], &aaddr[1], &aaddr[2],
                 &aaddr[3], &aaddr[4], &aaddr[5]) != 6)
        return 1;
    *btaddr = 0;
    for (unsigned int i : aaddr) {
        auto tmpaddr = static_cast<BTH_ADDR>(i & 0xff);
        *btaddr = ((*btaddr) << 8) + tmpaddr;
    }
    return 0;
}

int BluetoothHelper::ba2str(const BTH_ADDR btaddr, char *straddr) {
    unsigned char bytes[6]{};
    for (int i = 0; i < 6; i++)
        bytes[5 - i] = static_cast<unsigned char>((btaddr >> (i * 8)) & 0xff);
    if (sprintf_s(straddr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
                  bytes[0], bytes[1], bytes[2],
                  bytes[3], bytes[4], bytes[5]) != 6)
        return 1;
    return 0;
}

std::optional<SDPService> BluetoothHelper::RegisterSDPService(SOCKADDR address) {
    GUID guid = { 0x62182bf7, 0x97c8, 0x45f9, { 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xe0 } };
    auto service = new WSAQUERYSETW;
    std::memset(service, 0, sizeof(WSAQUERYSETW));
    service->dwSize = sizeof(WSAQUERYSETW);
    service->lpszServiceInstanceName = (LPWSTR)L"PC Bio Unlock BT";
    service->lpServiceClassId = &guid;
    service->dwNumberOfCsAddrs = 1;
    CSADDR_INFO csAddr{};
    csAddr.LocalAddr.iSockaddrLength = sizeof(address);
    csAddr.LocalAddr.lpSockaddr = (LPSOCKADDR)&address;
    csAddr.iSocketType = SOCK_STREAM;
    csAddr.iProtocol = BTHPROTO_RFCOMM;
    service->lpcsaBuffer = &csAddr;
    if (WSASetServiceW(service, RNRSERVICE_REGISTER, 0) != 0)
        return {};
    SDPService sdpService{};
    sdpService.handle = service;
    return sdpService;
}

bool BluetoothHelper::CloseSDPService(SDPService& service) {
    if (service.handle == nullptr)
        return false;
    if (WSASetServiceW((LPWSAQUERYSETW)service.handle, RNRSERVICE_DELETE, 0) != 0)
        return false;
    service.handle = nullptr;
    return true;
}
