#include "QRUtils.h"

#include <qrcodegen.hpp>
#include <sstream>

std::string QRUtils::GenerateSVG(const std::string &text) {
  auto border = 2;
  auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
  std::ostringstream sb{};
  sb << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  sb << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
  sb << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 )";
  sb << (qr.getSize() + border * 2) << " " << (qr.getSize() + border * 2) << "\" stroke=\"none\">\n";
  sb << "\t<rect width=\"100%\" height=\"100%\" fill=\"#FFFFFF\"/>\n";
  sb << "\t<path d=\"";
  for(int y = 0; y < qr.getSize(); y++) {
    for(int x = 0; x < qr.getSize(); x++) {
      if(qr.getModule(x, y)) {
        if(x != 0 || y != 0)
          sb << " ";
        sb << "M" << (x + border) << "," << (y + border) << "h1v1h-1z";
      }
    }
  }
  sb << "\" fill=\"#000000\"/>\n";
  sb << "</svg>\n";
  return sb.str();
}

std::string QRUtils::GenerateTerminal(const std::string &text) {
  auto border = 2;
  auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
  auto size = qr.getSize();
  auto isDark = [&](int x, int y) { return x >= 0 && y >= 0 && x < size && y < size && qr.getModule(x, y); };
  std::ostringstream sb{};
  for(int y = -border; y < size + border; y += 2) {
    sb << "\033[30;107m";
    for(int x = -border; x < size + border; x++) {
      auto top = isDark(x, y);
      auto bottom = isDark(x, y + 1);
      if(top && bottom)
        sb << "\xe2\x96\x88";
      else if(top)
        sb << "\xe2\x96\x80";
      else if(bottom)
        sb << "\xe2\x96\x84";
      else
        sb << " ";
    }
    sb << "\033[0m\n";
  }
  return sb.str();
}
