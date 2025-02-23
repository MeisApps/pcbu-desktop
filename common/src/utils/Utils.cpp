#include "Utils.h"

#include <chrono>

bool Utils::IsBigEndian() {
  short int number = 0x1;
  const auto numPtr = reinterpret_cast<char *>(&number);
  return numPtr[0] == 0;
}

int64_t Utils::GetCurrentTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
