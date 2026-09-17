#ifndef PCBU_DESKTOP_TLSCONTEXT_H
#define PCBU_DESKTOP_TLSCONTEXT_H

#include <boost/asio/ssl/context.hpp>

class TlsContext {
public:
  static boost::asio::ssl::context &Shared();
  static void Configure(boost::asio::ssl::context &ctx);

private:
  static void LoadSystemTrustStore(boost::asio::ssl::context &ctx);
  static void LoadDefaultPaths(boost::asio::ssl::context &ctx);

  TlsContext() = default;
};

#endif // PCBU_DESKTOP_TLSCONTEXT_H
