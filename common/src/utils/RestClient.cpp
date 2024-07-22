#include "RestClient.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

std::string RestClient::CheckForUpdates(const std::string &product, const std::string &platform) {
    httplib::Client client("https://api.meis-apps.com");
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);
    auto result = client.Get(fmt::format("/rest/appUpdater/checkUpdates?product={}&platform={}", product, platform));
    if(!result)
        throw std::runtime_error(to_string(result.error()));
    auto resultJson = nlohmann::json::parse(result->body);
    return resultJson["version"];
}
