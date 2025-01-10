#include "StringUtils.h"

#include <cstdint>
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <openssl/rand.h>

#ifdef WINDOWS
#include <Windows.h>
#endif

std::string StringUtils::Replace(const std::string &str, const std::string &from, const std::string &to) {
    if(from.empty())
        return str;
    std::string result = str;
    size_t startPos = 0;
    size_t toLen = to.empty() ? 1 : to.length();
    while((startPos = str.find(from, startPos)) != std::string::npos) {
        result.replace(startPos, from.length(), to);
        startPos += toLen;
    }
    return result;
}

std::vector<std::string> StringUtils::Split(const std::string &str, const std::string &delim) {
    size_t pos_start{}, pos_end{}, delim_len = delim.length();
    std::string token{};
    std::vector<std::string> res{};
    while ((pos_end = str.find(delim, pos_start)) != std::string::npos) {
        token = str.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }
    res.push_back(str.substr(pos_start));
    return res;
}

std::string StringUtils::LTrim(const std::string& str) {
    auto result = str;
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return result;
}

std::string StringUtils::RTrim(const std::string& str) {
    auto result = str;
    result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), result.end());
    return result;
}

std::string StringUtils::Trim(const std::string& str) {
    auto trim = RTrim(str);
    trim = LTrim(trim);
    return trim;
}

std::string StringUtils::ToLower(const std::string &str) {
    auto result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string StringUtils::RandomString(size_t len) {
    const std::string charset =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
            "!@#$%^&*()-_=+[]{}|;:,.<>?";
    std::vector<uint8_t> buffer(len);
    if (!RAND_bytes(buffer.data(), (int)len))
        return {};
    std::string result{};
    result.reserve(len);
    for (size_t i = 0; i < len; ++i)
        result += charset[buffer[i] % charset.size()];
    return result;
}

std::string StringUtils::WithSeperators(const std::string& str, const std::string& seperator, uint32_t groupSize) {
    std::vector<std::string> chunks{};
    for (size_t i = 0; i < str.size(); i += groupSize)
        chunks.push_back(str.substr(i, groupSize));
    return boost::algorithm::join(chunks, seperator);
}

std::string StringUtils::ToHexString(const std::vector<uint8_t> &data) {
    std::string hex;
    boost::algorithm::hex(data.begin(), data.end(), std::back_inserter(hex));
    return hex;
}

std::string StringUtils::ToBase32String(const std::string& str) {
    const std::string BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string encodedStr{};
    uint32_t buffer{};
    uint32_t bitsLeft{};
    for (uint8_t c : str) {
        buffer = (buffer << 8) | c;
        bitsLeft += 8;
        while (bitsLeft >= 5) {
            encodedStr += BASE32_ALPHABET[(buffer >> (bitsLeft - 5)) & 0x1F];
            bitsLeft -= 5;
        }
    }
    if (bitsLeft > 0)
        encodedStr += BASE32_ALPHABET[(buffer << (5 - bitsLeft)) & 0x1F];
    return encodedStr;
}

#ifdef WINDOWS
std::wstring StringUtils::ToWideString(const std::string &string) {
    if (string.empty())
        return L"";
    const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, &string.at(0), static_cast<int>(string.size()), nullptr, 0);
    if (size_needed <= 0)
        return L"";
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &string.at(0), static_cast<int>(string.size()), &result.at(0), size_needed);
    return result;
}

std::string StringUtils::FromWideString(const std::wstring &wide_string) {
    if (wide_string.empty())
        return "";
    const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), static_cast<int>(wide_string.size()), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
        return "";
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), static_cast<int>(wide_string.size()), &result.at(0), size_needed, nullptr, nullptr);
    return result;
}
#endif
