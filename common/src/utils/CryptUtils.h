#ifndef PCBU_DESKTOP_CRYPTUTILS_H
#define PCBU_DESKTOP_CRYPTUTILS_H

#include <cstdint>
#include <string>
#include <vector>

#define CRYPT_PACKET_TIMEOUT (60000 * 2)

enum PacketCryptResult { OK, INVALID_TIMESTAMP, OTHER_ERROR };

struct CryptPacket {
  PacketCryptResult result{};
  std::vector<uint8_t> data{};
};

class CryptUtils {
public:
  static std::string Sha256(const std::string &text);
  static CryptPacket EncryptAESPacket(const std::vector<uint8_t> &data, const std::string &pwd);
  static CryptPacket DecryptAESPacket(const std::vector<uint8_t> &data, const std::string &pwd);

private:
  static size_t EncryptAES(const uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstLen, const char *pwd);
  static size_t DecryptAES(const uint8_t *src, size_t srcLen, uint8_t *dst, size_t dstLen, const char *pwd);
  static uint8_t *GenerateKey(const char *pwd, unsigned char *salt);

  CryptUtils() = default;
};

#endif // PCBU_DESKTOP_CRYPTUTILS_H
