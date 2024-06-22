#include "CryptUtils.h"

#include "Utils.h"
#include "StringUtils.h"

#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#ifdef WINDOWS
#include <WinSock2.h>
#elif LINUX
#include <arpa/inet.h>
#define htonll(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define ntohll(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
#endif

#define AES_KEY_SIZE 256
#define IV_SIZE 16
#define SALT_SIZE 16
#define GCM_TAG_SIZE 16
#define ITERATIONS 65535
#define CRYPT_BUFFER_SIZE 2048

std::string CryptUtils::Sha256(const std::string &text) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr)
        return {};
    if (!EVP_DigestInit_ex(context, EVP_sha3_256(), nullptr)) {
        EVP_MD_CTX_free(context);
        return {};
    }
    if (!EVP_DigestUpdate(context, text.data(), text.size())) {
        EVP_MD_CTX_free(context);
        return {};
    }
    std::vector<uint8_t> hash(EVP_MD_size(EVP_sha3_256()));
    uint32_t length = 0;
    if (!EVP_DigestFinal_ex(context, hash.data(), &length)) {
        EVP_MD_CTX_free(context);
        return {};
    }
    EVP_MD_CTX_free(context);
    return StringUtils::ToHexString(hash);
}

CryptPacket CryptUtils::EncryptAESPacket(const std::vector<uint8_t>& data, const std::string& pwd) {
    auto dataBuf = (uint8_t *)malloc(data.size() + sizeof(int64_t));
    if(dataBuf == nullptr)
        return {PacketCryptResult::OTHER_ERROR};

    auto timeMs = htonll(Utils::GetCurrentTimeMs());
    memcpy(dataBuf, &timeMs, sizeof(int64_t));
    memcpy(&dataBuf[sizeof(int64_t)], data.data(), data.size());

    auto encBuffer = (uint8_t *)malloc(CRYPT_BUFFER_SIZE);
    if(encBuffer == nullptr)
        return {PacketCryptResult::OTHER_ERROR};

    auto encLen = EncryptAES(dataBuf, data.size() + sizeof(int64_t), encBuffer, CRYPT_BUFFER_SIZE, pwd.c_str());
    free(dataBuf);
    if(encLen <= sizeof(int64_t)) {
        free(encBuffer);
        return {PacketCryptResult::OTHER_ERROR};
    }

    std::vector<uint8_t> resultVec(encLen);
    memcpy(resultVec.data(), encBuffer, encLen);
    free(encBuffer);
    return {PacketCryptResult::OK, resultVec};
}

CryptPacket CryptUtils::DecryptAESPacket(const std::vector<uint8_t>& data, const std::string& pwd) {
    auto decBuffer = (uint8_t *)malloc(CRYPT_BUFFER_SIZE);
    auto decLen = DecryptAES(data.data(), data.size(), decBuffer, CRYPT_BUFFER_SIZE, pwd.c_str());
    if(decLen <= sizeof(int64_t)) {
        free(decBuffer);
        return {PacketCryptResult::OTHER_ERROR};
    }
    decLen = decLen - sizeof(int64_t);

    int64_t timestamp{};
    memcpy(&timestamp, decBuffer, sizeof(int64_t));
    timestamp = (int64_t)ntohll(timestamp);

    auto timeDiff = Utils::GetCurrentTimeMs() - timestamp;
    if(timeDiff < -CRYPT_PACKET_TIMEOUT || timeDiff > CRYPT_PACKET_TIMEOUT) {
        free(decBuffer);
        return {PacketCryptResult::INVALID_TIMESTAMP};
    }

    std::vector<uint8_t> resultVec(decLen);
    memcpy(resultVec.data(), &decBuffer[sizeof(int64_t)], decLen);
    free(decBuffer);
    return {PacketCryptResult::OK, resultVec};
}

size_t CryptUtils::EncryptAES(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen, const char* pwd) {
    ERR_print_errors_fp(stderr);
    int status;
    if (!src || !dst || dstLen < srcLen + SALT_SIZE + IV_SIZE + GCM_TAG_SIZE)
        return false;

    unsigned char iv[IV_SIZE]{};
    if (RAND_bytes(iv, IV_SIZE) != 1)
        return false;
    memcpy(dst, iv, IV_SIZE);

    unsigned char salt[SALT_SIZE]{};
    if (RAND_bytes(salt, SALT_SIZE) != 1)
        return false;
    memcpy(dst + IV_SIZE, salt, SALT_SIZE);

    unsigned char* key = GenerateKey(pwd, salt);
    if (!key)
        return false;

    int numberOfBytes = 0;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if(!ctx) {
        free(key);
        return false;
    }

    status = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    if (!status) {
        free(key);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    status = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr);
    if (!status) {
        free(key);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    status = EVP_CIPHER_CTX_set_padding(ctx, 0);
    if(!status) {
        free(key);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    status = EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv);
    free(key);
    if (!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    status = EVP_EncryptUpdate(ctx, dst + IV_SIZE + SALT_SIZE, &numberOfBytes, src, (int)srcLen);
    if (!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int encryptedLen = numberOfBytes;
    status = EVP_EncryptFinal_ex(ctx, dst + IV_SIZE + SALT_SIZE + encryptedLen, &numberOfBytes);
    if (!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    status = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, dst + IV_SIZE + SALT_SIZE + encryptedLen);
    if(!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    EVP_CIPHER_CTX_free(ctx);
    return IV_SIZE + SALT_SIZE + encryptedLen + GCM_TAG_SIZE;
}

size_t CryptUtils::DecryptAES(const uint8_t* src, size_t srcLen, uint8_t* dst, size_t dstLen, const char* pwd) {
    ERR_print_errors_fp(stderr);
    int status;
    if (!src || !dst || srcLen < IV_SIZE + SALT_SIZE + GCM_TAG_SIZE || dstLen < srcLen - IV_SIZE - SALT_SIZE - GCM_TAG_SIZE)
        return false;

    unsigned char iv[IV_SIZE]{};
    memcpy(iv, src, IV_SIZE);

    unsigned char salt[SALT_SIZE]{};
    memcpy(salt, src + IV_SIZE, SALT_SIZE);

    unsigned char* key = GenerateKey(pwd, salt);
    if (!key)
        return false;

    int numberOfBytes = 0;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if(!ctx) {
        free(key);
        return false;
    }

    status = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    if (!status) {
        free(key);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    status = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr);
    if (!status) {
        free(key);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    status = EVP_CIPHER_CTX_set_padding(ctx, 0);
    if(!status) {
        free(key);
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }
    status = EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, iv);
    free(key);
    if (!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    status = EVP_DecryptUpdate(ctx, dst, &numberOfBytes, src + IV_SIZE + SALT_SIZE, (int)srcLen - IV_SIZE - SALT_SIZE - GCM_TAG_SIZE);
    if (!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int decryptedLen = numberOfBytes;
    status = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, (void*)(src + srcLen - GCM_TAG_SIZE));
    if (!status) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    status = EVP_DecryptFinal_ex(ctx, dst + decryptedLen, &numberOfBytes);
    EVP_CIPHER_CTX_free(ctx);
    if (!status)
        return false;
    return decryptedLen;
}

uint8_t* CryptUtils::GenerateKey(const char* pwd, unsigned char* salt) {
    auto key = (unsigned char*)malloc(AES_KEY_SIZE / 8);
    if (!key)
        return nullptr;
    if (PKCS5_PBKDF2_HMAC(pwd, (int)strlen(pwd), salt, SALT_SIZE, ITERATIONS, EVP_sha256(), AES_KEY_SIZE / 8, key) != 1) {
        free(key);
        return nullptr;
    }
    return key;
}
