#ifndef PAM_PCBIOUNLOCK_UNLOCKSTATE_H
#define PAM_PCBIOUNLOCK_UNLOCKSTATE_H

#include <string>
#include "utils/I18n.h"

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
    UNK_ERROR = 10
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
        } else if(state == UnlockState::UNK_ERROR) {
            return I18n::Get("unlock_error_unknown");
        } else {
            return I18n::Get("error_unknown");
        }
    }

private:
    UnlockStateUtils() = default;
};

#endif //PAM_PCBIOUNLOCK_UNLOCKSTATE_H
