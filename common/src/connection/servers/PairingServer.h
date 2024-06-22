#ifndef PCBU_DESKTOP_PAIRINGSERVER_H
#define PCBU_DESKTOP_PAIRINGSERVER_H

#include <vector>
#include <boost/asio.hpp>
#include "nlohmann/json.hpp"

#include "PairingStructs.h"

namespace boostnet = boost::asio::ip;

class PairingServer {
public:
    PairingServer();
    ~PairingServer();

    void Start(const PairingServerData& serverData);
    void Stop();

private:
    void OnAccept();

    std::vector<uint8_t> ReadPacket();
    void WritePacket(const std::string& dataStr);
    void WritePacket(const std::vector<uint8_t>& data);

    boost::asio::io_service m_IOService{};
    boostnet::tcp::acceptor m_Acceptor;
    boostnet::tcp::socket m_Socket;

    PairingServerData m_ServerData{};
    std::thread m_AcceptThread{};
    std::atomic<bool> m_IsRunning{};
};


#endif //PCBU_DESKTOP_PAIRINGSERVER_H
