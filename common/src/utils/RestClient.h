#ifndef PCBU_DESKTOP_RESTCLIENT_H
#define PCBU_DESKTOP_RESTCLIENT_H

#include <cstdint>
#include <string>

class RestClient {
public:
    static std::string CheckForUpdates(const std::string& product, const std::string& platform);

private:
    RestClient() = default;
};

#endif //PCBU_DESKTOP_RESTCLIENT_H
