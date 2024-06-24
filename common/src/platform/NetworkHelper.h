#ifndef PCBU_DESKTOP_NETWORKHELPER_H
#define PCBU_DESKTOP_NETWORKHELPER_H

#include <string>
#include <vector>

struct NetworkInterface {
    std::string ifName{};
    std::string ipAddress{};
    std::string macAddress{};
};

class NetworkHelper {
public:
    static std::vector<NetworkInterface> GetLocalNetInterfaces();
    static std::string GetHostName();
    static bool HasLANConnection();

private:
    NetworkHelper() = default;
};


#endif //PCBU_DESKTOP_NETWORKHELPER_H
