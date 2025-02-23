#ifndef PCBU_DESKTOP_UTILS_H
#define PCBU_DESKTOP_UTILS_H

#include <cstdint>
#include <vector>

class Utils {
public:
  static bool IsBigEndian();
  static int64_t GetCurrentTimeMs();

private:
  Utils() = default;
};

#endif // PCBU_DESKTOP_UTILS_H
