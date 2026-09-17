#include "CloudPairingServer.h"

#include "utils/I18n.h"

CloudPairingServer::~CloudPairingServer() {
  Stop();
}

void CloudPairingServer::Stop() {
  m_IsRunning = false;
  if(m_RelayStream)
    m_RelayStream->Close();
  if(m_RelayThread.joinable())
    m_RelayThread.join();
  m_RelayStream.reset();
}

bool CloudPairingServer::ConnectRelay(const std::string &relayUrl, const std::string &sessionId, const std::string &joinToken) {
  if(m_RelayThread.joinable())
    return false;
  m_RelayStream = std::make_unique<WebSocketStream>();
  m_RelayThread = std::thread(&CloudPairingServer::RelayThread, this, relayUrl, sessionId, joinToken);
  return true;
}

void CloudPairingServer::RelayThread(const std::string &relayUrl, const std::string &sessionId, const std::string &joinToken) {
  spdlog::info("Connecting to the pairing relay...");
  if(!m_RelayStream->Connect(relayUrl, sessionId, joinToken, "pc")) {
    spdlog::error("Failed connecting to the pairing relay.");
    ReportError(I18n::Get("error_cloud_connect"));
    return;
  }
  spdlog::info("Cloud pairing client connected.");
  ++m_NumConnections;
  HandleClient(*m_RelayStream);
  --m_NumConnections;
  m_RelayStream->Close();
  spdlog::info("Cloud pairing client closed.");
}
