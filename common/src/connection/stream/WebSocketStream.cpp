#include "WebSocketStream.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include <boost/url.hpp>
#include <nlohmann/json.hpp>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

#include "connection/web/TlsContext.h"
#include "utils/AppInfo.h"

namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;
namespace urls = boost::urls;

constexpr std::uint16_t RELAY_VERSION_CLOSE_CODE = 4001;
constexpr std::uint16_t RELAY_SESSION_TIMEOUT_CLOSE_CODE = 4002;
constexpr std::uint16_t RELAY_PEER_UNAVAILABLE_CLOSE_CODE = 4003;

constexpr auto CONNECT_TIMEOUT = std::chrono::seconds(10);
constexpr auto IDLE_TIMEOUT = std::chrono::seconds(20);
constexpr std::size_t MAX_MESSAGE_SIZE = 128 * 1024;

bool WebSocketStream::Connect(const std::string &relayUrl, const std::string &sessionId, const std::string &joinToken, const std::string &role) {
  auto parsed = urls::parse_uri(relayUrl);
  if(!parsed) {
    spdlog::error("Invalid relay URL: {}", relayUrl);
    return false;
  }
  const urls::url_view url = *parsed;
  const auto scheme = url.scheme_id();
  if(scheme != urls::scheme::ws && scheme != urls::scheme::wss) {
    spdlog::error("Unsupported relay URL scheme: {}", relayUrl);
    return false;
  }
  const bool secure = scheme == urls::scheme::wss;
  const auto host = url.host();
  const auto portStr = url.has_port() ? std::string(url.port()) : std::string(secure ? "443" : "80");
  const auto target = url.encoded_target().empty() ? std::string("/") : std::string(url.encoded_target());
  const auto hostHeader = std::string(url.encoded_host_and_port());

  try {
    auto const results = Resolve(host, portStr);
    if(m_Closed)
      throw std::runtime_error("Canceled.");

    nlohmann::json join = {{"sid", sessionId}, {"token", joinToken}, {"role", role}, {"version", AppInfo::GetRelayProtocolVersion()}};
    auto joinStr = join.dump();

    if(secure) {
      m_Wss = std::make_unique<WssStream>(m_Ctx, TlsContext::Shared());
      if(!SSL_set_tlsext_host_name(m_Wss->next_layer().native_handle(), host.c_str()))
        throw std::runtime_error("Failed to set TLS SNI host name.");

      ConnectTo(*m_Wss, results);
      m_Wss->next_layer().set_verify_mode(ssl::verify_peer);
      m_Wss->next_layer().set_verify_callback(ssl::host_name_verification(host));

      auto slot = MakeSlot();
      beast::get_lowest_layer(*m_Wss).expires_after(CONNECT_TIMEOUT);
      m_Wss->next_layer().async_handshake(ssl::stream_base::client, [slot](beast::error_code e) { *slot = e; });
      auto ec = Drive(slot);
      if(ec)
        throw std::runtime_error("TLS handshake failed: " + ec.message());

      FinishHandshake(*m_Wss, hostHeader, target, joinStr);
    } else {
      m_Ws = std::make_unique<WsStream>(m_Ctx);
      ConnectTo(*m_Ws, results);
      FinishHandshake(*m_Ws, hostHeader, target, joinStr);
    }
    return !m_Closed;
  } catch(const std::exception &ex) {
    if(m_Closed) {
      spdlog::info("Cloud relay connect canceled.");
      return false;
    }
    spdlog::error("Cloud relay connect failed: {}", ex.what());
    return false;
  }
}

StreamResult WebSocketStream::Read(uint8_t *buffer, size_t length) {
  if(m_Closed)
    return {0, PacketError::CLOSED_CONNECTION};
  if(m_Wss)
    return ReadFrom(*m_Wss, buffer, length);
  if(m_Ws)
    return ReadFrom(*m_Ws, buffer, length);
  return {0, PacketError::CLOSED_CONNECTION};
}

StreamResult WebSocketStream::WriteRaw(const uint8_t *buffer, size_t length) {
  if(m_Closed)
    return {0, PacketError::CLOSED_CONNECTION};
  if(m_Wss)
    return WriteTo(*m_Wss, buffer, length);
  if(m_Ws)
    return WriteTo(*m_Ws, buffer, length);
  return {0, PacketError::CLOSED_CONNECTION};
}

void WebSocketStream::Close() {
  if(m_Closed.exchange(true))
    return;
  net::post(m_Ctx, [this] { CloseSockets(); });
}

void WebSocketStream::CloseSockets() {
  if(m_Resolver)
    m_Resolver->cancel();
  if(m_Wss)
    beast::get_lowest_layer(*m_Wss).close();
  else if(m_Ws)
    beast::get_lowest_layer(*m_Ws).close();
}

WebSocketStream::ErrorSlot WebSocketStream::MakeSlot() {
  return std::make_shared<beast::error_code>(net::error::would_block);
}

bool WebSocketStream::RunUntilComplete(const ErrorSlot &slot) {
  while(*slot == net::error::would_block) {
    if(m_Ctx.stopped())
      m_Ctx.restart();
    if(m_Ctx.run_one() == 0)
      return false;
  }
  return true;
}

beast::error_code WebSocketStream::Drive(const ErrorSlot &slot) {
  if(RunUntilComplete(slot))
    return *slot;
  spdlog::error("Relay operation did not complete. Aborting...");
  CloseSockets();
  if(!RunUntilComplete(slot)) {
    spdlog::critical("Relay operation could not be aborted.");
    *slot = net::error::operation_aborted;
  }
  return *slot;
}

net::ip::tcp::resolver::results_type WebSocketStream::Resolve(const std::string &host, const std::string &port) {
  m_Resolver = std::make_unique<net::ip::tcp::resolver>(m_Ctx);
  auto slot = MakeSlot();
  auto results = std::make_shared<net::ip::tcp::resolver::results_type>();
  m_Resolver->async_resolve(host, port, [slot, results](beast::error_code e, const net::ip::tcp::resolver::results_type &r) {
    *slot = e;
    *results = r;
  });

  m_Ctx.restart();
  m_Ctx.run_for(CONNECT_TIMEOUT);
  if(!m_Ctx.stopped()) {
    m_Resolver->cancel();
    m_Ctx.run();
  }
  m_Resolver.reset();
  if(*slot == net::error::would_block)
    throw std::runtime_error("Relay host lookup did not complete.");
  if(*slot)
    throw std::runtime_error("Relay host lookup failed: " + slot->message());
  return *results;
}

template <class Stream> void WebSocketStream::ConnectTo(Stream &stream, const net::ip::tcp::resolver::results_type &results) {
  auto slot = MakeSlot();
  beast::get_lowest_layer(stream).expires_after(CONNECT_TIMEOUT);
  beast::get_lowest_layer(stream).async_connect(results, [slot](beast::error_code e, const net::ip::tcp::endpoint &) { *slot = e; });
  auto ec = Drive(slot);
  if(ec)
    throw std::runtime_error("Relay connect failed: " + ec.message());

  beast::error_code optEc{};
  beast::get_lowest_layer(stream).socket().set_option(net::ip::tcp::no_delay(true), optEc);
  if(optEc)
    spdlog::warn("set_option(TCP_NODELAY) failed. ({})", optEc.message());
}

template <class Stream>
void WebSocketStream::FinishHandshake(Stream &stream, const std::string &hostHeader, const std::string &target, const std::string &joinStr) {
  beast::get_lowest_layer(stream).expires_never();
  stream.set_option(websocket::stream_base::timeout{CONNECT_TIMEOUT, IDLE_TIMEOUT, true});
  stream.set_option(websocket::stream_base::decorator([](websocket::request_type &req) { req.set(http::field::user_agent, "pcbu-desktop"); }));
  stream.read_message_max(MAX_MESSAGE_SIZE);

  auto slot = MakeSlot();
  stream.async_handshake(hostHeader, target, [slot](beast::error_code e) { *slot = e; });
  auto ec = Drive(slot);
  if(ec)
    throw std::runtime_error("Relay handshake failed: " + ec.message());

  stream.text(true);
  slot = MakeSlot();
  stream.async_write(net::buffer(joinStr), [slot](beast::error_code e, std::size_t) { *slot = e; });
  ec = Drive(slot);
  if(ec)
    throw std::runtime_error("Writing relay join message failed: " + ec.message());
  stream.binary(true);
}

template <class Stream> StreamResult WebSocketStream::ReadFrom(Stream &stream, uint8_t *out, size_t length) {
  while(m_ReadOffset >= m_ReadBuffer.size()) {
    m_ReadBuffer.consume(m_ReadBuffer.size());
    m_ReadOffset = 0;
    auto slot = MakeSlot();
    stream.async_read(m_ReadBuffer, [slot](beast::error_code e, std::size_t) { *slot = e; });
    auto ec = Drive(slot);
    if(ec)
      return {0, MapError(ec, stream.reason().code)};
  }
  auto available = m_ReadBuffer.size() - m_ReadOffset;
  auto count = std::min(available, length);
  const auto *data = static_cast<const uint8_t *>(m_ReadBuffer.cdata().data());
  std::memcpy(out, data + m_ReadOffset, count);
  m_ReadOffset += count;
  return {static_cast<int>(count), PacketError::NONE};
}

template <class Stream> StreamResult WebSocketStream::WriteTo(Stream &stream, const uint8_t *in, size_t length) {
  auto slot = MakeSlot();
  stream.async_write(net::buffer(in, length), [slot](beast::error_code e, std::size_t) { *slot = e; });
  auto ec = Drive(slot);
  if(ec)
    return {0, MapError(ec, stream.reason().code)};
  return {static_cast<int>(length), PacketError::NONE};
}

PacketError WebSocketStream::MapError(const beast::error_code &ec, std::uint16_t closeCode) {
  if(ec == beast::error::timeout || ec == net::error::timed_out)
    return PacketError::TIMEOUT;
  switch(closeCode) {
    case RELAY_VERSION_CLOSE_CODE:
      return PacketError::UNSUPPORTED_VERSION;
    case RELAY_SESSION_TIMEOUT_CLOSE_CODE:
      return PacketError::TIMEOUT;
    case RELAY_PEER_UNAVAILABLE_CLOSE_CODE:
      return PacketError::PEER_UNAVAILABLE;
    default:
      break;
  }
  return PacketError::CLOSED_CONNECTION;
}
