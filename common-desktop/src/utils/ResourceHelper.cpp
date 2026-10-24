#include "ResourceHelper.h"

#ifdef WINDOWS
#include <Windows.h>
#endif

std::vector<uint8_t> ResourceHelper::GetResource(const std::string &url) {
  auto data = GetResourceData(url);
  if(data.empty())
    throw std::runtime_error(fmt::format("Failed to open resource '{}'.", url));
  return {data.begin(), data.end()};
}

#ifdef WINDOWS
std::span<const uint8_t> ResourceHelper::LoadRcData(int id) {
  auto resInfo = FindResource(nullptr, MAKEINTRESOURCE(id), RT_RCDATA);
  if(!resInfo)
    return {};
  auto resData = LoadResource(nullptr, resInfo);
  if(!resData)
    return {};
  return {static_cast<const uint8_t *>(LockResource(resData)), SizeofResource(nullptr, resInfo)};
}
#endif
