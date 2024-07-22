#include "BTUnlockClient.h"

BTUnlockClient::BTUnlockClient(const std::string& deviceAddress, const PairedDevice& device)
        : BaseUnlockConnection(device) {
#warning Not implemented.
}

bool BTUnlockClient::Start() {
    return false;
}

void BTUnlockClient::Stop() {

}

void BTUnlockClient::ConnectThread() {

}
