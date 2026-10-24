#include "LibBluetooth.h"

#include <spdlog/spdlog.h>

decltype(&::str2ba) LibBluetooth::str2ba{};
decltype(&::ba2str) LibBluetooth::ba2str{};

decltype(&::hci_get_route) LibBluetooth::hci_get_route{};
decltype(&::hci_open_dev) LibBluetooth::hci_open_dev{};
decltype(&::hci_inquiry) LibBluetooth::hci_inquiry{};
decltype(&::hci_read_remote_name) LibBluetooth::hci_read_remote_name{};

decltype(&::sdp_connect) LibBluetooth::sdp_connect{};
decltype(&::sdp_close) LibBluetooth::sdp_close{};
decltype(&::sdp_uuid16_create) LibBluetooth::sdp_uuid16_create{};
decltype(&::sdp_uuid128_create) LibBluetooth::sdp_uuid128_create{};
decltype(&::sdp_uuid_to_proto) LibBluetooth::sdp_uuid_to_proto{};
decltype(&::sdp_list_append) LibBluetooth::sdp_list_append{};
decltype(&::sdp_list_free) LibBluetooth::sdp_list_free{};
decltype(&::sdp_service_search_attr_req) LibBluetooth::sdp_service_search_attr_req{};
decltype(&::sdp_get_access_protos) LibBluetooth::sdp_get_access_protos{};
decltype(&::sdp_record_alloc) LibBluetooth::sdp_record_alloc{};
decltype(&::sdp_record_register) LibBluetooth::sdp_record_register{};
decltype(&::sdp_set_service_id) LibBluetooth::sdp_set_service_id{};
decltype(&::sdp_set_uuidseq_attr) LibBluetooth::sdp_set_uuidseq_attr{};
decltype(&::sdp_set_access_protos) LibBluetooth::sdp_set_access_protos{};
decltype(&::sdp_set_info_attr) LibBluetooth::sdp_set_info_attr{};
decltype(&::sdp_data_alloc) LibBluetooth::sdp_data_alloc{};
decltype(&::sdp_data_free) LibBluetooth::sdp_data_free{};

boost::dll::shared_library LibBluetooth::g_Library{};
std::once_flag LibBluetooth::g_LoadFlag{};
bool LibBluetooth::g_IsLoaded{};

bool LibBluetooth::Load() {
  std::call_once(g_LoadFlag, []() { g_IsLoaded = LoadSymbols(); });
  return g_IsLoaded;
}

bool LibBluetooth::LoadSymbols() {
  for(const auto &libName : {"libbluetooth.so.3", "libbluetooth.so"}) {
    std::error_code ec{};
    g_Library.load(libName, ec, boost::dll::load_mode::search_system_folders);
    if(!ec)
      break;
  }
  if(!g_Library.is_loaded()) {
    spdlog::warn("libbluetooth not found, Bluetooth is unavailable.");
    return false;
  }

  try {
    Resolve(str2ba, "str2ba");
    Resolve(ba2str, "ba2str");

    Resolve(hci_get_route, "hci_get_route");
    Resolve(hci_open_dev, "hci_open_dev");
    Resolve(hci_inquiry, "hci_inquiry");
    Resolve(hci_read_remote_name, "hci_read_remote_name");

    Resolve(sdp_connect, "sdp_connect");
    Resolve(sdp_close, "sdp_close");
    Resolve(sdp_uuid16_create, "sdp_uuid16_create");
    Resolve(sdp_uuid128_create, "sdp_uuid128_create");
    Resolve(sdp_uuid_to_proto, "sdp_uuid_to_proto");
    Resolve(sdp_list_append, "sdp_list_append");
    Resolve(sdp_list_free, "sdp_list_free");
    Resolve(sdp_service_search_attr_req, "sdp_service_search_attr_req");
    Resolve(sdp_get_access_protos, "sdp_get_access_protos");
    Resolve(sdp_record_alloc, "sdp_record_alloc");
    Resolve(sdp_record_register, "sdp_record_register");
    Resolve(sdp_set_service_id, "sdp_set_service_id");
    Resolve(sdp_set_uuidseq_attr, "sdp_set_uuidseq_attr");
    Resolve(sdp_set_access_protos, "sdp_set_access_protos");
    Resolve(sdp_set_info_attr, "sdp_set_info_attr");
    Resolve(sdp_data_alloc, "sdp_data_alloc");
    Resolve(sdp_data_free, "sdp_data_free");
  } catch(const std::exception &ex) {
    spdlog::error("Failed loading libbluetooth symbols: {}", ex.what());
    g_Library.unload();
    return false;
  }
  return true;
}
