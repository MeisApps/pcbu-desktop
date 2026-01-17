#ifndef PCBU_DESKTOP_STRINGUTILS_H
#define PCBU_DESKTOP_STRINGUTILS_H

#include <cstdint>
#include <string>
#include <vector>

class StringUtils {
public:
  static std::string Replace(const std::string &str, const std::string &from, const std::string &to);
  static std::vector<std::string> Split(const std::string &str, const std::string &delim);
  static std::string LTrim(const std::string &str);
  static std::string RTrim(const std::string &str);
  static std::string Trim(const std::string &str);
  static std::string ToLower(const std::string &str);
  static std::string Truncate(const std::string &str, uint32_t maxLen, const std::string &ellipsis = "...");

  static std::string RandomString(size_t len);
  static std::string WithSeperators(const std::string &str, const std::string &seperator, uint32_t groupSize);
  static std::vector<uint8_t> FromHexString(const std::string& hex);
  static std::string ToHexString(const std::vector<uint8_t> &data);
  static std::string ToBase32String(const std::string &str);

#ifdef WINDOWS
  static std::wstring ToWideString(const std::string &string);
  static std::string FromWideString(const std::wstring &wide_string);
#endif

private:
  StringUtils() = default;
};

#endif // PCBU_DESKTOP_STRINGUTILS_H
