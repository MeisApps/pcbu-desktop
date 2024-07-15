#include "BluetoothHelper.h"

#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <spdlog/spdlog.h>

bool BluetoothHelper::IsAvailable() {
    int dev_id = hci_get_route(nullptr);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0)
        return false;
    close(sock);
    return true;
}

void BluetoothHelper::StartScan() {
}

void BluetoothHelper::StopScan() {
}

std::vector<BluetoothDevice> BluetoothHelper::ScanDevices() {
    int dev_id = hci_get_route(nullptr);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0)
        return {};

    int len = 8;
    int max_rsp = 255;
    auto ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
    int num_rsp = hci_inquiry(dev_id, len, max_rsp, nullptr, &ii, IREQ_CACHE_FLUSH);
    if (num_rsp <= 0) {
        spdlog::error("hci_inquiry() failed.");
        free(ii);
        close(sock);
        return {};
    }

    std::vector<BluetoothDevice> result{};
    for (int i = 0; i < num_rsp; i++) {
        char addr[18]{};
        char name[248]{};
        ba2str(&(ii + i)->bdaddr, addr);
        memset(name, 0, sizeof(name));
        if (hci_read_remote_name(sock, &(ii + i)->bdaddr, sizeof(name), name, 0) < 0)
            strcpy(name, "Unknown device");
        result.push_back({name, addr});
    }
    free(ii);
    close(sock);
    return result;
}

bool BluetoothHelper::PairDevice(const BluetoothDevice &device) {
    return true;
}

int BluetoothHelper::FindSDPChannel(const std::string& deviceAddr, uint8_t *uuid) {
    bdaddr_t address;
    str2ba(deviceAddr.c_str(), &address);
    auto bdAny = (bdaddr_t){ {0, 0, 0, 0, 0, 0} };
    sdp_session_t* session = sdp_connect(&bdAny, &address, SDP_RETRY_IF_BUSY);
    if (!session) {
        spdlog::error("sdp_connect() failed. (Code={})", errno);
        return -1;
    }
    uuid_t uuid128;
    sdp_uuid128_create(&uuid128, uuid);
    int range = 0x0000ffff;
    sdp_list_t* responseList;
    sdp_list_t* searchList = sdp_list_append(nullptr, &uuid128);
    sdp_list_t* attrIdList = sdp_list_append(nullptr, &range);
    int success = sdp_service_search_attr_req(
            session, searchList, SDP_ATTR_REQ_RANGE, attrIdList, &responseList);
    if (success) {
        spdlog::error("sdp_service_search_attr_req() failed. (Code={})", success);
        return -1;
    }
    success = sdp_list_len(responseList);
    if (success <= 0) {
        spdlog::error("sdp_list_len() failed. (Code={})", success);
        return -1;
    }
    int channel = 0;
    sdp_list_t* responses = responseList;
    while (responses) {
        auto record = (sdp_record_t*)responses->data;
        sdp_list_t* protoList;
        success = sdp_get_access_protos(record, &protoList);
        if (success) {
            spdlog::error("sdp_get_access_protos() failed. (Code={})", success);
            return -1;
        }
        sdp_list_t* protocol = protoList;
        while (protocol) {
            sdp_list_t* pds;
            int protocolCount = 0;
            pds = (sdp_list_t*)protocol->data;
            while (pds) {
                sdp_data_t* d;
                int dtd;
                d = (sdp_data_t*)(pds->data);
                while (d) {
                    dtd = d->dtd;
                    switch (dtd) {
                        case SDP_UUID16:
                        case SDP_UUID32:
                        case SDP_UUID128:
                            protocolCount = sdp_uuid_to_proto(&d->val.uuid);
                            break;
                        case SDP_UINT8:
                            if (protocolCount == RFCOMM_UUID)
                                channel = d->val.uint8; // Got channel id
                            break;
                        default:
                            break;
                    }
                    d = d->next;
                }
                pds = pds->next;
            }
            sdp_list_free((sdp_list_t*)protocol->data, nullptr);
            protocol = protocol->next;
        }
        sdp_list_free(protoList, nullptr);
        responses = responses->next;
    }
    if (channel <= 0 || channel > 30) {
        spdlog::error("Could not find SDP channel.");
        return -1;
    }
    return channel;
}
