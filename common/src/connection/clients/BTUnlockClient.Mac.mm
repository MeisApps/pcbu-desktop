#include "BTUnlockClient.h"

#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>

@interface BTUnlockClientWrapper : NSObject <IOBluetoothRFCOMMChannelDelegate>
@property (nonatomic, strong) IOBluetoothDevice *device;
@property (nonatomic, strong) IOBluetoothRFCOMMChannel *socket;

- (id)initWithAddress:(NSString *)deviceAddress;

- (void)start;
- (void)stop;
@end

@implementation BTUnlockClientWrapper
- (id)initWithAddress:(NSString *)deviceAddress {
    self.device = [IOBluetoothDevice deviceWithAddressString:deviceAddress];
    return self;
}

- (void)start {
    [self.device performSDPQuery:self];
}

- (void)stop {
    [self.socket closeChannel];
}

- (void)sdpQueryComplete:(IOBluetoothDevice *)device status:(IOReturn)status {
    if (status != kIOReturnSuccess) {
        spdlog::error("SDP query failed. (Code={})", status);
        return;
    }
    static uint8_t CHANNEL_UUID[16] = { 0x62, 0x18, 0x2b, 0xf7, 0x97, 0xc8,
                                        0x45, 0xf9, 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xdf };
    auto svcUUID = [IOBluetoothSDPUUID uuidWithBytes:CHANNEL_UUID length:16];
    auto svcRecord = [device getServiceRecordForUUID:svcUUID];
    if(!svcRecord) {
        spdlog::error("SDP getServiceRecordForUUID failed.");
        return;
    }
    BluetoothRFCOMMChannelID channelID{};
    if((status = [svcRecord getRFCOMMChannelID:&channelID]) != kIOReturnSuccess) {
        spdlog::error("SDP getRFCOMMChannelID failed. (Code={})", status);
        return;
    }
    if((status = [device openRFCOMMChannelSync:&_socket withChannelID:channelID delegate:self]) != kIOReturnSuccess) {
        spdlog::error("Bluetooth connect failed. (Code={})", status);
        return;
    }
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength {
    NSData *data = [NSData dataWithBytes:dataPointer length:dataLength];
    NSString *receivedString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    NSLog(@"Received data: %@", receivedString);
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel {
    NSLog(@"RFCOMM Channel closed.");
    self.socket = nil;
}
@end

BTUnlockClient::BTUnlockClient(const std::string& deviceAddress, const PairedDevice& device)
        : BaseUnlockConnection(device) {
    #warning WIP // ToDo
    m_Wrapper = [[BTUnlockClientWrapper alloc] initWithAddress:[NSString stringWithCString:deviceAddress.c_str() encoding:NSUTF8StringEncoding]];
    //[self.socket writeSync:(void *)data.bytes length:data.length];
}

bool BTUnlockClient::Start() {
    if(m_IsRunning)
        return true;

    m_IsRunning = true;
    [(BTUnlockClientWrapper *)m_Wrapper start];
    return true;
}

void BTUnlockClient::Stop() {
    if(!m_IsRunning)
        return;

    m_IsRunning = false;
    m_HasConnection = false;
    [(BTUnlockClientWrapper *)m_Wrapper stop];
}
