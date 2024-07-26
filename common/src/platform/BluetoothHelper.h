#ifndef PCBU_DESKTOP_BLUETOOTHHELPER_H
#define PCBU_DESKTOP_BLUETOOTHHELPER_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#ifdef WINDOWS
#include <WinSock2.h>
#include <ws2bth.h>
#include <bluetoothapis.h>
#endif

struct BluetoothDevice {
    std::string name{};
    std::string address{};
};

struct SDPService {
    void *handle{};
};

class BluetoothHelper {
public:
    static bool IsAvailable();

    static void StartScan();
    static void StopScan();
    static std::vector<BluetoothDevice> ScanDevices();

    static bool PairDevice(const BluetoothDevice& device);

#if defined(WINDOWS) || defined(LINUX)
    static bool CloseSDPService(SDPService& service);

#ifdef LINUX
    static std::optional<SDPService> RegisterSDPService();

    static int FindSDPChannel(const std::string& deviceAddr, uint8_t *uuid);
#elif WINDOWS
    static std::optional<SDPService> RegisterSDPService(SOCKADDR address);

    static int str2ba(const char* straddr, BTH_ADDR* btaddr);
    static int ba2str(BTH_ADDR btaddr, char* straddr);
#endif
#endif

private:
    BluetoothHelper() = default;

#ifdef APPLE
    static void *g_InquiryDelegate;
    static void *g_DeviceInquiry;
#endif
};

#endif //PCBU_DESKTOP_BLUETOOTHHELPER_H
