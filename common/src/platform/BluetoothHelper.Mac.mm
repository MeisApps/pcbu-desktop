#include "BluetoothHelper.h"

#include <thread>
#include <spdlog/spdlog.h>

#import <IOBluetooth/IOBluetooth.h>

void *BluetoothHelper::g_DeviceInquiry{};
void *BluetoothHelper::g_InquiryDelegate{};

@interface BluetoothScannerDelegate : NSObject<IOBluetoothDeviceInquiryDelegate>
@property (atomic, strong) NSMutableArray *foundDevices;
@end

@implementation BluetoothScannerDelegate
- (instancetype)init {
    self = [super init];
    if (self) {
        _foundDevices = [NSMutableArray array];
    }
    return self;
}

- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry *)sender device:(IOBluetoothDevice *)device {
    [_foundDevices addObject:device];
}
@end

bool BluetoothHelper::IsAvailable() {
    @autoreleasepool {
        return [[IOBluetoothHostController defaultController] powerState] == kBluetoothHCIPowerStateON;
    }
}

void BluetoothHelper::StartScan() {
    g_InquiryDelegate = [[BluetoothScannerDelegate alloc] init];
    g_DeviceInquiry = [IOBluetoothDeviceInquiry inquiryWithDelegate:(id)g_InquiryDelegate];
    auto inquiry = (IOBluetoothDeviceInquiry *)g_DeviceInquiry;
    [inquiry start];
}

void BluetoothHelper::StopScan() {
    auto delegate = (BluetoothScannerDelegate *)g_InquiryDelegate;
    auto inquiry = (IOBluetoothDeviceInquiry *)g_DeviceInquiry;
    [inquiry stop];
    //[inquiry release];
    //[delegate release];
    g_InquiryDelegate = nullptr;
    g_DeviceInquiry = nullptr;
}

std::vector<BluetoothDevice> BluetoothHelper::ScanDevices() {
    std::vector<BluetoothDevice> result{};
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    if(g_InquiryDelegate == nullptr) {
        spdlog::error("Error: Scan not started");
        return result;
    }
    auto delegate = (BluetoothScannerDelegate *)g_InquiryDelegate;
    for (IOBluetoothDevice *device in delegate.foundDevices) {
        NSString *deviceAddr = [device addressString];
        if (deviceAddr) {
            BluetoothDevice dev{};
            dev.address = std::string([deviceAddr UTF8String]);
            auto deviceName = [device name];
            if(deviceName)
                dev.name = std::string([deviceName UTF8String]);
            else
                dev.name = "Unknown device";
            result.emplace_back(dev);
        }
    }
    return result;
}

bool BluetoothHelper::PairDevice(const BluetoothDevice &device) {
    /*@autoreleasepool { // ToDo
        auto ioDevice = [IOBluetoothDevice deviceWithAddressString:[NSString stringWithUTF8String:device.address.c_str()]];
        auto pair = [IOBluetoothDevicePair pairWithDevice:ioDevice];
        [pair start];
    }*/
    return false;
}
