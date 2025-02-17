#include <unistd.h>

#include "storage/LoggingSystem.h"
#include "platform/PlatformHelper.h"
#include "handler/UnlockHandler.h"

int runMain(int argc, char *argv[]) {
    if(argc != 3) {
        printf("Invalid parameters.\n");
        return -1;
    }
    auto userName = argv[1];
    auto serviceName = argv[2];
    std::function<void (const std::string&)> printMessage = [](const std::string& s) {
        printf("%s\n", s.c_str());
    };
    auto handler = UnlockHandler(printMessage);
    auto result = handler.GetResult(userName, serviceName);
    if(result.state == UnlockState::SUCCESS) {
        if(strcmp(userName, result.device.userName.c_str()) == 0) {
            if(PlatformHelper::CheckLogin(userName, result.password) == PlatformLoginResult::SUCCESS) {
                // pam_set_item(pamh, PAM_AUTHTOK, result.additionalData.c_str()); ToDo
                return 0;
            }
        }

        auto errorMsg = I18n::Get("error_password");
        printf("%s\n", errorMsg.c_str());
        return 1;
    } else if(result.state == UnlockState::CANCELED) {
        return 1;
    } else if(result.state == UnlockState::TIMEOUT || result.state == UnlockState::CONNECT_ERROR) {
        return -1;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if(setuid(0) != 0) {
        printf("setuid(0) failed.\n");
        return -1;
    }
    LoggingSystem::Init("module", false);
    auto result = runMain(argc, argv);
    LoggingSystem::Destroy();
    return result;
}
