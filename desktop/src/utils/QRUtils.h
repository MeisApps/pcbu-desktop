#ifndef PCBU_DESKTOP_QRUTILS_H
#define PCBU_DESKTOP_QRUTILS_H

#include <string>

class QRUtils {
public:
  static std::string GenerateSVG(const std::string &text);

private:
  QRUtils() = default;
};

#endif // PCBU_DESKTOP_QRUTILS_H
