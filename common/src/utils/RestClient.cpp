#include "RestClient.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "AppInfo.h"
#include "connection/web/HttpClient.h"

const std::string WEBSITE_URL = "https://meis-apps.com";

const HttpOptions REST_OPTIONS = {.connectTimeout = std::chrono::seconds(5), .transferTimeout = std::chrono::seconds(10)};

std::string RestClient::CheckForUpdates(const std::string &product, const std::string &platform) {
  auto client = HttpClient();
  auto url = fmt::format("{}/rest/checkUpdates?product={}&platform={}&currVersion={}", WEBSITE_URL, product, platform, AppInfo::GetVersion());
  auto result = client.Get(url, REST_OPTIONS);
  if(!result.ok || result.status != 200)
    throw std::runtime_error(result.error);
  auto resultJson = nlohmann::json::parse(result.body);
  return resultJson["version"];
}

CloudPairingPollResult RestClient::PollCloudPairing(const std::string &pollSecret, HttpClient *client) {
  try {
    auto localClient = client == nullptr ? std::make_unique<HttpClient>() : nullptr;
    auto &http = client == nullptr ? *localClient : *client;

    nlohmann::json body = {{"pollSecret", pollSecret}};
    auto result = http.Post(WEBSITE_URL + "/rest/pcbu/pollCloudPairing", body.dump(), "application/json", REST_OPTIONS);
    if(!result.ok || result.status >= 500)
      return {CloudPairingStatus::ServerUnreachable};
    if(result.status == 204)
      return {CloudPairingStatus::Pending};
    if(result.status == 400)
      return {CloudPairingStatus::UnsupportedVersion};
    if(result.status != 200) {
      spdlog::error("Cloud pairing poll error. (Status={})", result.status);
      return {CloudPairingStatus::Error};
    }
    auto json = nlohmann::json::parse(result.body);
    if(json.value("result", "") != "OK")
      return {CloudPairingStatus::Error};

    auto info = CloudRelayInfo();
    info.sessionId = json.value("sessionId", "");
    info.relayUrl = json.value("relayUrl", "");
    info.joinToken = json.value("joinToken", "");
    if(info.sessionId.empty() || info.relayUrl.empty() || info.joinToken.empty())
      return {CloudPairingStatus::Error};
    return {CloudPairingStatus::Ok, info};
  } catch(const std::exception &ex) {
    spdlog::error("Cloud pairing poll error: {}", ex.what());
    return {CloudPairingStatus::Error};
  }
}

CloudUnlockResult RestClient::RequestCloudUnlock(const std::string &cloudToken, HttpClient *client) {
  try {
    auto localClient = client == nullptr ? std::make_unique<HttpClient>() : nullptr;
    auto &http = client == nullptr ? *localClient : *client;

    nlohmann::json body = {{"token", cloudToken}};
    auto result = http.Post(WEBSITE_URL + "/rest/pcbu/cloudUnlock", body.dump(), "application/json", REST_OPTIONS);
    if(!result.ok || result.status >= 500)
      return {CloudUnlockStatus::ServerUnreachable};
    switch(result.status) {
      case 400:
        return {CloudUnlockStatus::UnsupportedVersion};
      case 401:
        return {CloudUnlockStatus::InvalidToken};
      case 402: {
        auto resultStr = std::string();
        try {
          resultStr = nlohmann::json::parse(result.body).value("result", "");
        } catch(const std::exception &) {
        }
        return {resultStr == "SUBSCRIPTION_STALE" ? CloudUnlockStatus::SubscriptionStale : CloudUnlockStatus::NoSubscription};
      }
      case 404:
        return {CloudUnlockStatus::DeviceUnknown};
      case 409:
        return {CloudUnlockStatus::DeviceStale};
      case 410:
        return {CloudUnlockStatus::PairingRevoked};
      case 424:
        return {CloudUnlockStatus::PhoneUnreachable};
      case 429:
        return {CloudUnlockStatus::RateLimited};
      default:
        break;
    }
    if(result.status != 200) {
      spdlog::error("Cloud unlock request error. (Status={})", result.status);
      return {CloudUnlockStatus::Error};
    }
    auto json = nlohmann::json::parse(result.body);
    if(json.value("result", "") != "OK")
      return {CloudUnlockStatus::Error};

    auto info = CloudRelayInfo();
    info.sessionId = json.value("sessionId", "");
    info.relayUrl = json.value("relayUrl", "");
    info.joinToken = json.value("joinToken", "");
    if(info.sessionId.empty() || info.relayUrl.empty() || info.joinToken.empty())
      return {CloudUnlockStatus::Error};
    return {CloudUnlockStatus::Ok, info};
  } catch(const std::exception &ex) {
    spdlog::error("Cloud unlock request error: {}", ex.what());
    return {CloudUnlockStatus::Error};
  }
}
