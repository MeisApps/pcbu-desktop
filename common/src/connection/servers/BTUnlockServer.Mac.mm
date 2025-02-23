#include "BTUnlockServer.h"

BTUnlockServer::BTUnlockServer(const PairedDevice &device) : BaseUnlockConnection(device) {
#warning Not implemented.
}

bool BTUnlockServer::Start() {
  return false;
}

void BTUnlockServer::Stop() {}

void BTUnlockServer::PerformAuthFlow(SOCKET socket) {
  BaseUnlockConnection::PerformAuthFlow(socket);
}
