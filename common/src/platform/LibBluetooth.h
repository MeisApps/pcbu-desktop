#ifndef PCBU_DESKTOP_LIBBLUETOOTH_H
#define PCBU_DESKTOP_LIBBLUETOOTH_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <boost/dll/shared_library.hpp>
#include <mutex>
#include <type_traits>

class LibBluetooth {
public:
  static bool Load();

  static decltype(&::str2ba) str2ba;
  static decltype(&::ba2str) ba2str;

  static decltype(&::hci_get_route) hci_get_route;
  static decltype(&::hci_open_dev) hci_open_dev;
  static decltype(&::hci_inquiry) hci_inquiry;
  static decltype(&::hci_read_remote_name) hci_read_remote_name;

  static decltype(&::sdp_connect) sdp_connect;
  static decltype(&::sdp_close) sdp_close;
  static decltype(&::sdp_uuid16_create) sdp_uuid16_create;
  static decltype(&::sdp_uuid128_create) sdp_uuid128_create;
  static decltype(&::sdp_uuid_to_proto) sdp_uuid_to_proto;
  static decltype(&::sdp_list_append) sdp_list_append;
  static decltype(&::sdp_list_free) sdp_list_free;
  static decltype(&::sdp_service_search_attr_req) sdp_service_search_attr_req;
  static decltype(&::sdp_get_access_protos) sdp_get_access_protos;
  static decltype(&::sdp_record_alloc) sdp_record_alloc;
  static decltype(&::sdp_record_register) sdp_record_register;
  static decltype(&::sdp_set_service_id) sdp_set_service_id;
  static decltype(&::sdp_set_uuidseq_attr) sdp_set_uuidseq_attr;
  static decltype(&::sdp_set_access_protos) sdp_set_access_protos;
  static decltype(&::sdp_set_info_attr) sdp_set_info_attr;
  static decltype(&::sdp_data_alloc) sdp_data_alloc;
  static decltype(&::sdp_data_free) sdp_data_free;

private:
  LibBluetooth() = default;

  static bool LoadSymbols();
  template <typename T> static void Resolve(T &func, const char *name) {
    func = &g_Library.get<std::remove_pointer_t<T>>(name);
  }

  static boost::dll::shared_library g_Library;
  static std::once_flag g_LoadFlag;
  static bool g_IsLoaded;
};

#endif // PCBU_DESKTOP_LIBBLUETOOTH_H
