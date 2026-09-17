#ifndef PCBU_DESKTOP_CLOUDUNLOCKCLIENT_H
#define PCBU_DESKTOP_CLOUDUNLOCKCLIENT_H

#include <memory>

#include "connection/stream/WebSocketStream.h"
#include "connection/unlock/BaseUnlockConnection.h"
#include "connection/web/HttpClient.h"
#include "utils/RestClient.h"

class CloudUnlockClient : public BaseUnlockConnection {
public:
  explicit CloudUnlockClient(const PairedDevice &device);
  ~CloudUnlockClient() override;

  bool Start() override;
  void Stop() override;

private:
  void ConnectThread();

  static UnlockState MapRequestError(CloudUnlockStatus status);

  std::unique_ptr<WebSocketStream> m_Stream{};
  std::unique_ptr<HttpClient> m_HttpClient{};
};

#endif // PCBU_DESKTOP_CLOUDUNLOCKCLIENT_H
