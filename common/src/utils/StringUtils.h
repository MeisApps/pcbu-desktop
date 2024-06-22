#ifndef PCBU_DESKTOP_STRINGUTILS_H
#define PCBU_DESKTOP_STRINGUTILS_H

#include <string>
#include <vector>
#include <cstdint>

class StringUtils {
public:
    static std::string Replace(const std::string& str, const std::string& from, const std::string& to);
    static std::vector<std::string> Split(const std::string& str, const std::string& delim);
    static std::string LTrim(const std::string& str);
    static std::string RTrim(const std::string& str);
    static std::string Trim(const std::string& str);
    static std::string ToLower(const std::string& str);

    static std::string RandomString(size_t len);
    static std::string ToHexString(const std::vector<uint8_t>& data);

#ifdef WINDOWS
    static std::wstring ToWideString(const std::string& string);
    static std::string FromWideString(const std::wstring& wide_string);
#endif

private:
    StringUtils() = default;
};


#endif //PCBU_DESKTOP_STRINGUTILS_H
