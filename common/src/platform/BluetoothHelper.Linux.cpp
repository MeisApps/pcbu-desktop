#include "BluetoothHelper.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

bool BluetoothHelper::IsAvailable() {
  int dev_id = hci_get_route(nullptr);
  int sock = hci_open_dev(dev_id);
  if(dev_id < 0 || sock < 0)
    return false;
  close(sock);
  return true;
}

void BluetoothHelper::StartScan() {}

void BluetoothHelper::StopScan() {}

std::vector<BluetoothDevice> BluetoothHelper::ScanDevices() {
  int dev_id = hci_get_route(nullptr);
  int sock = hci_open_dev(dev_id);
  if(dev_id < 0 || sock < 0)
    return {};

  int len = 8;
  int max_rsp = 255;
  auto ii = (inquiry_info *)malloc(max_rsp * sizeof(inquiry_info));
  int num_rsp = hci_inquiry(dev_id, len, max_rsp, nullptr, &ii, IREQ_CACHE_FLUSH);
  if(num_rsp <= 0) {
    spdlog::error("hci_inquiry() failed.");
    free(ii);
    close(sock);
    return {};
  }

  std::vector<BluetoothDevice> result{};
  for(int i = 0; i < num_rsp; i++) {
    char addr[18]{};
    char name[248]{};
    ba2str(&(ii + i)->bdaddr, addr);
    memset(name, 0, sizeof(name));
    if(hci_read_remote_name(sock, &(ii + i)->bdaddr, sizeof(name), name, 0) < 0)
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

int BluetoothHelper::FindSDPChannel(const std::string &deviceAddr, uint8_t *uuid) {
  bdaddr_t address;
  if(str2ba(deviceAddr.c_str(), &address) != 0) {
    spdlog::error("Invalid Bluetooth address format: {}", deviceAddr);
    return -1;
  }

  bdaddr_t bdAny = {{0, 0, 0, 0, 0, 0}};
  sdp_session_t *session = sdp_connect(&bdAny, &address, SDP_RETRY_IF_BUSY);
  if(!session) {
    spdlog::error("sdp_connect() failed. (Code={})", errno);
    return -1;
  }

  uuid_t uuid128;
  sdp_uuid128_create(&uuid128, uuid);
  uint32_t range = 0x0000ffff;
  sdp_list_t *responseList = nullptr;
  sdp_list_t *searchList = sdp_list_append(nullptr, &uuid128);
  sdp_list_t *attrIdList = sdp_list_append(nullptr, &range);

  int success = sdp_service_search_attr_req(session, searchList, SDP_ATTR_REQ_RANGE, attrIdList, &responseList);
  sdp_list_free(searchList, nullptr);
  sdp_list_free(attrIdList, nullptr);
  if(success != 0) {
    spdlog::error("sdp_service_search_attr_req() failed. (Code={})", errno);
    sdp_close(session);
    return -1;
  }

  success = sdp_list_len(responseList);
  if(success <= 0) {
    spdlog::error("sdp_list_len() failed. (Code={})", success);
    sdp_close(session);
    return -1;
  }

  int channel = 0;
  sdp_list_t *responses = responseList;
  while(responses) {
    auto record = (sdp_record_t *)responses->data;
    sdp_list_t *protoList = nullptr;
    success = sdp_get_access_protos(record, &protoList);
    if(success != 0) {
      spdlog::error("sdp_get_access_protos() failed. (Code={})", success);
      sdp_close(session);
      sdp_list_free(responseList, nullptr);
      return -1;
    }

    sdp_list_t *protocol = protoList;
    while(protocol) {
      auto pds = (sdp_list_t *)protocol->data;
      int protocolCount = 0;
      while(pds) {
        auto d = (sdp_data_t *)pds->data;
        while(d) {
          switch(d->dtd) {
            case SDP_UUID16:
            case SDP_UUID32:
            case SDP_UUID128:
              protocolCount = sdp_uuid_to_proto(&d->val.uuid);
              break;
            case SDP_UINT8:
              if(protocolCount == RFCOMM_UUID)
                channel = d->val.uint8;
              break;
            default:
              break;
          }
          d = d->next;
        }
        pds = pds->next;
      }
      protocol = protocol->next;
    }
    sdp_list_free(protoList, nullptr);
    responses = responses->next;
  }
  sdp_list_free(responseList, nullptr);
  sdp_close(session);

  if(channel <= 0 || channel > 30) {
    spdlog::error("Could not find valid SDP channel.");
    return -1;
  }
  return channel;
}

std::optional<SDPService> BluetoothHelper::RegisterSDPService(uint8_t channel) { // ToDo: More error checks
  static uint8_t CHANNEL_UUID[16] = {0x62, 0x18, 0x2b, 0xf7, 0x97, 0xc8, 0x45, 0xf9, 0xaa, 0x2c, 0x53, 0xc5, 0xf2, 0x00, 0x8b, 0xdf};
  uuid_t rootUUID{}, l2capUUID{}, rfcommUUID{}, svcUUID{};
  sdp_list_t *l2capList{}, *rfcommList{}, *rootList{}, *protoList{}, *accessProtoList{};
  sdp_data_t *channelData{};
  sdp_record_t *record = sdp_record_alloc();

  sdp_uuid128_create(&svcUUID, &CHANNEL_UUID);
  sdp_set_service_id(record, svcUUID);

  sdp_uuid16_create(&rootUUID, PUBLIC_BROWSE_GROUP);
  rootList = sdp_list_append(nullptr, &rootUUID);
  sdp_set_browse_groups(record, rootList);

  sdp_uuid16_create(&l2capUUID, L2CAP_UUID);
  l2capList = sdp_list_append(nullptr, &l2capUUID);
  protoList = sdp_list_append(nullptr, l2capList);

  sdp_uuid16_create(&rfcommUUID, RFCOMM_UUID);
  channelData = sdp_data_alloc(SDP_UINT8, &channel);
  rfcommList = sdp_list_append(nullptr, &rfcommUUID);
  sdp_list_append(rfcommList, channelData);
  sdp_list_append(protoList, rfcommList);

  accessProtoList = sdp_list_append(nullptr, protoList);
  sdp_set_access_protos(record, accessProtoList);
  sdp_set_info_attr(record, "PC Bio Unlock BT", "", "");

  bdaddr_t local = {{0, 0, 0, 0xff, 0xff, 0xff}};
  bdaddr_t localAny = {{0, 0, 0, 0, 0, 0}};
  auto session = sdp_connect(&localAny, &local, SDP_RETRY_IF_BUSY);
  if(sdp_record_register(session, record, SDP_RECORD_PERSIST) < 0) {
    spdlog::error("sdp_record_register() failed.");
  }

  sdp_data_free(channelData);
  sdp_list_free(accessProtoList, nullptr);
  sdp_list_free(rfcommList, nullptr);
  sdp_list_free(protoList, nullptr);
  sdp_list_free(l2capList, nullptr);
  sdp_list_free(rootList, nullptr);

  if(session == nullptr)
    return {};
  SDPService sdpService{};
  sdpService.handle = session;
  return sdpService;
}

bool BluetoothHelper::CloseSDPService(SDPService &service) {
  if(service.handle == nullptr)
    return false;
  if(sdp_close((sdp_session_t *)service.handle) < 0)
    return false;
  service.handle = nullptr;
  return true;
}
