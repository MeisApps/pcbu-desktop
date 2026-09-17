#ifndef PAM_PCBIOUNLOCK_UNLOCKSTATE_H
#define PAM_PCBIOUNLOCK_UNLOCKSTATE_H

#include "utils/I18n.h"
#include <string>

enum UnlockState {
  UNKNOWN = 0,
  SUCCESS = 1,
  CANCELED = 2,
  TIMEOUT = 3,
  CONNECT_ERROR = 4,
  TIME_ERROR = 5,
  DATA_ERROR = 6,
  NOT_PAIRED_ERROR = 7,
  APP_ERROR = 8,
  START_ERROR = 9,
  PORT_ERROR = 10,
  PROTOCOL_ERROR = 11,
  CLOUD_SERVER_UNREACHABLE = 12,
  CLOUD_PHONE_UNREACHABLE = 13,
  CLOUD_RATE_LIMITED = 14,
  CLOUD_PAIRING_GONE = 15,
  CLOUD_NO_SUBSCRIPTION = 16,
  CLOUD_SUBSCRIPTION_STALE = 17,
  CLOUD_DEVICE_STALE = 18,
  UNK_ERROR = 19,
};

class UnlockStateUtils {
public:
  static std::string ToString(const UnlockState state) {
    if(state == UnlockState::SUCCESS) {
      return I18n::Get("unlock_success");
    } else if(state == UnlockState::CANCELED) {
      return I18n::Get("unlock_canceled");
    } else if(state == UnlockState::TIMEOUT) {
      return I18n::Get("unlock_timeout");
    } else if(state == UnlockState::CONNECT_ERROR) {
      return I18n::Get("unlock_error_connect");
    } else if(state == UnlockState::TIME_ERROR) {
      return I18n::Get("unlock_error_time");
    } else if(state == UnlockState::DATA_ERROR) {
      return I18n::Get("unlock_error_data");
    } else if(state == UnlockState::NOT_PAIRED_ERROR) {
      return I18n::Get("unlock_error_not_paired");
    } else if(state == UnlockState::APP_ERROR) {
      return I18n::Get("unlock_error_app");
    } else if(state == UnlockState::START_ERROR) {
      return I18n::Get("error_start_handler");
    } else if(state == UnlockState::PORT_ERROR) {
      return I18n::Get("error_unlock_server_init");
    } else if(state == UnlockState::PROTOCOL_ERROR) {
      return I18n::Get("error_protocol_mismatch");
    } else if(state == UnlockState::UNK_ERROR) {
      return I18n::Get("unlock_error_unknown");
    } else if(state == UnlockState::CLOUD_SERVER_UNREACHABLE) {
      return I18n::Get("unlock_error_cloud_server_unreachable");
    } else if(state == UnlockState::CLOUD_PHONE_UNREACHABLE) {
      return I18n::Get("unlock_error_cloud_phone_unreachable");
    } else if(state == UnlockState::CLOUD_RATE_LIMITED) {
      return I18n::Get("unlock_error_cloud_rate_limited");
    } else if(state == UnlockState::CLOUD_NO_SUBSCRIPTION) {
      return I18n::Get("unlock_error_cloud_no_subscription");
    } else if(state == UnlockState::CLOUD_PAIRING_GONE) {
      return I18n::Get("unlock_error_cloud_pairing_gone");
    } else if(state == UnlockState::CLOUD_SUBSCRIPTION_STALE) {
      return I18n::Get("unlock_error_cloud_subscription_stale");
    } else if(state == UnlockState::CLOUD_DEVICE_STALE) {
      return I18n::Get("unlock_error_cloud_device_stale");
    } else {
      return I18n::Get("error_unknown");
    }
  }

private:
  UnlockStateUtils() = default;
};

enum class UnlockPhase {
  FINISHED,
  STARTING,
  CLIENT_CONNECTING,
  SERVER_WAITING,
  CLOUD_REQUESTING,
  CLOUD_CONNECTING,
  PHONE_UNLOCKING,
};

class UnlockPhaseUtils {
public:
  static std::string ToString(const UnlockPhase phase) {
    if(phase == UnlockPhase::STARTING) {
      return I18n::Get("initializing");
    } else if(phase == UnlockPhase::CLIENT_CONNECTING) {
      return I18n::Get("wait_client_phone_connect");
    } else if(phase == UnlockPhase::SERVER_WAITING) {
      return I18n::Get("wait_server_phone_connect");
    } else if(phase == UnlockPhase::CLOUD_REQUESTING) {
      return I18n::Get("wait_cloud_request");
    } else if(phase == UnlockPhase::CLOUD_CONNECTING) {
      return I18n::Get("wait_cloud_connect");
    } else if(phase == UnlockPhase::PHONE_UNLOCKING) {
      return I18n::Get("wait_phone_unlock");
    }
    return {};
  }

private:
  UnlockPhaseUtils() = default;
};

#endif // PAM_PCBIOUNLOCK_UNLOCKSTATE_H
