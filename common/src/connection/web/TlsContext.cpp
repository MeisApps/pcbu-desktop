#include "TlsContext.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <spdlog/spdlog.h>

#ifdef WINDOWS
#include <wincrypt.h>
#include <windows.h>
#undef X509_NAME
#undef X509_EXTENSIONS
#undef X509_CERT_PAIR
#undef PKCS7_ISSUER_AND_SERIAL
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#elif defined(APPLE)
#include <map>
#include <string>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#else
#include <algorithm>
#include <cctype>
#include <filesystem>
#endif

namespace ssl = boost::asio::ssl;

ssl::context &TlsContext::Shared() {
  static ssl::context ctx = [] {
    ssl::context context{ssl::context::tls_client};
    Configure(context);
    return context;
  }();
  return ctx;
}

void TlsContext::Configure(ssl::context &ctx) {
  ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3 | ssl::context::no_tlsv1 |
                  ssl::context::no_tlsv1_1);
  SSL_CTX_set_min_proto_version(ctx.native_handle(), TLS1_2_VERSION);
  ctx.set_verify_mode(ssl::verify_peer);
  LoadSystemTrustStore(ctx);
}

void TlsContext::LoadDefaultPaths(ssl::context &ctx) {
  boost::system::error_code ec{};
  ctx.set_default_verify_paths(ec);
  if(ec)
    spdlog::warn("No root certificates could be loaded: {}.", ec.message());
}

#ifdef WINDOWS
void TlsContext::LoadSystemTrustStore(ssl::context &ctx) {
  auto *sysStore = CertOpenSystemStoreW(static_cast<HCRYPTPROV_LEGACY>(0), L"ROOT");
  if(sysStore == nullptr) {
    spdlog::warn("Failed to open the Windows root certificate store. (Code={})", GetLastError());
    LoadDefaultPaths(ctx);
    return;
  }
  auto *store = SSL_CTX_get_cert_store(ctx.native_handle());
  auto loaded = 0;
  PCCERT_CONTEXT certContext = nullptr;
  while((certContext = CertEnumCertificatesInStore(sysStore, certContext)) != nullptr) {
    const auto *encoded = static_cast<const unsigned char *>(certContext->pbCertEncoded);
    auto *cert = d2i_X509(nullptr, &encoded, static_cast<long>(certContext->cbCertEncoded));
    if(cert != nullptr) {
      if(X509_STORE_add_cert(store, cert) == 1) // ignores duplicates
        ++loaded;
      X509_free(cert);
    }
  }
  CertCloseStore(sysStore, 0);
  ERR_clear_error();
  if(loaded == 0) {
    spdlog::warn("No system root certificates could be loaded, falling back to the OpenSSL defaults.");
    LoadDefaultPaths(ctx);
  } else {
    spdlog::debug("Loaded {} system root certificates.", loaded);
  }
}
#elif defined(APPLE)
enum class TrustVerdict { Unspecified, Trusted, Denied };

static bool AppliesToSsl(CFDictionaryRef entry) {
  auto policy = (SecPolicyRef)CFDictionaryGetValue(entry, kSecTrustSettingsPolicy);
  if(policy == nullptr)
    return true;
  auto properties = SecPolicyCopyProperties(policy);
  if(properties == nullptr)
    return false;
  const void *oid{};
  auto isSsl = CFDictionaryGetValueIfPresent(properties, kSecPolicyOid, &oid) && oid != nullptr && CFEqual(oid, kSecPolicyAppleSSL);
  CFRelease(properties);
  return isSsl;
}

static TrustVerdict GetTrustVerdict(SecCertificateRef cert, SecTrustSettingsDomain domain) {
  CFArrayRef settings{};
  if(SecTrustSettingsCopyTrustSettings(cert, domain, &settings) != errSecSuccess || settings == nullptr)
    return TrustVerdict::Unspecified;

  const auto count = CFArrayGetCount(settings);
  auto verdict = count == 0 ? TrustVerdict::Trusted : TrustVerdict::Unspecified;
  for(CFIndex i = 0; i < count && verdict != TrustVerdict::Denied; ++i) {
    auto entry = (CFDictionaryRef)CFArrayGetValueAtIndex(settings, i);
    if(entry == nullptr || !AppliesToSsl(entry))
      continue;
    auto result = kSecTrustSettingsResultTrustRoot;
    int32_t resultValue{};
    auto value = (CFNumberRef)CFDictionaryGetValue(entry, kSecTrustSettingsResult);
    if(value != nullptr && CFNumberGetValue(value, kCFNumberSInt32Type, &resultValue))
      result = (SecTrustSettingsResult)resultValue;
    if(result == kSecTrustSettingsResultDeny)
      verdict = TrustVerdict::Denied;
    else if(result == kSecTrustSettingsResultTrustRoot || result == kSecTrustSettingsResultTrustAsRoot)
      verdict = TrustVerdict::Trusted;
  }
  CFRelease(settings);
  return verdict;
}

void TlsContext::LoadSystemTrustStore(ssl::context &ctx) {
  static const SecTrustSettingsDomain DOMAINS[] = {kSecTrustSettingsDomainSystem, kSecTrustSettingsDomainAdmin, kSecTrustSettingsDomainUser};

  std::map<std::string, bool> anchors{};
  for(const auto domain : DOMAINS) {
    CFArrayRef certs{};
    if(SecTrustSettingsCopyCertificates(domain, &certs) != errSecSuccess || certs == nullptr)
      continue;
    for(CFIndex i = 0; i < CFArrayGetCount(certs); ++i) {
      auto cert = (SecCertificateRef)CFArrayGetValueAtIndex(certs, i);
      const auto verdict = GetTrustVerdict(cert, domain);
      if(verdict == TrustVerdict::Unspecified)
        continue;
      auto certData = SecCertificateCopyData(cert);
      if(certData == nullptr)
        continue;
      const auto *bytes = reinterpret_cast<const char *>(CFDataGetBytePtr(certData));
      anchors[std::string(bytes, static_cast<std::size_t>(CFDataGetLength(certData)))] = verdict == TrustVerdict::Trusted;
      CFRelease(certData);
    }
    CFRelease(certs);
  }

  auto *store = SSL_CTX_get_cert_store(ctx.native_handle());
  auto loaded = 0;
  auto denied = 0;
  for(const auto &[der, trusted] : anchors) {
    if(!trusted) {
      ++denied;
      continue;
    }
    const auto *encoded = reinterpret_cast<const unsigned char *>(der.data());
    auto *x509 = d2i_X509(nullptr, &encoded, static_cast<long>(der.size()));
    if(x509 == nullptr)
      continue;
    if(X509_STORE_add_cert(store, x509) == 1) // ignores duplicates
      ++loaded;
    X509_free(x509);
  }
  ERR_clear_error();

  if(denied > 0)
    spdlog::debug("Skipped {} distrusted system certificates.", denied);
  if(loaded == 0) {
    spdlog::warn("No system root certificates could be loaded, falling back to the OpenSSL defaults.");
    LoadDefaultPaths(ctx);
  } else {
    spdlog::debug("Loaded {} system root certificates.", loaded);
  }
}
#else
static bool HasHashedCerts(const std::filesystem::path &dir) {
  std::error_code ec{};
  std::filesystem::directory_iterator it{dir, ec};
  const std::filesystem::directory_iterator end{};
  for(; !ec && it != end; it.increment(ec)) {
    const auto ext = it->path().extension().string();
    if(ext.size() > 1 && std::all_of(ext.begin() + 1, ext.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
      return true;
  }
  return false;
}

void TlsContext::LoadSystemTrustStore(ssl::context &ctx) {
  static const char *CA_FILES[] = {
      "/etc/ssl/certs/ca-certificates.crt", // Debian/Ubuntu
      "/etc/pki/tls/certs/ca-bundle.crt",   // RHEL/CentOS
      "/etc/ssl/ca-bundle.pem",             // openSUSE
      "/etc/pki/tls/cacert.pem",            // OpenELEC
      "/etc/ssl/cert.pem",                  // Alpine
  };
  static const char *CA_DIRS[] = {
      "/etc/ssl/certs",             // Debian/Ubuntu
      "/etc/pki/tls/certs",         // RHEL/CentOS
      "/usr/share/ca-certificates", // Other
  };

  boost::system::error_code ec{};
  std::error_code fsEc{};
  for(const auto *caFile : CA_FILES) {
    if(!std::filesystem::exists(caFile, fsEc))
      continue;
    ctx.load_verify_file(caFile, ec);
    if(!ec) {
      spdlog::debug("Loaded system root certificates from '{}'.", caFile);
      return;
    }
  }
  for(const auto *caDir : CA_DIRS) {
    if(!std::filesystem::is_directory(caDir, fsEc) || !HasHashedCerts(caDir))
      continue;
    ctx.add_verify_path(caDir, ec);
    if(!ec) {
      spdlog::debug("Loaded system root certificates from '{}'.", caDir);
      return;
    }
  }
  LoadDefaultPaths(ctx);
}
#endif
