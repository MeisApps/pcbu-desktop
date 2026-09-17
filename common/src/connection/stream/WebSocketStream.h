#ifndef PCBU_DESKTOP_WEBSOCKETSTREAM_H
#define PCBU_DESKTOP_WEBSOCKETSTREAM_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "connection/stream/ConnectionStream.h"

class WebSocketStream : public ConnectionStream {
public:
  WebSocketStream() = default;
  ~WebSocketStream() override = default;

  bool Connect(const std::string &relayUrl, const std::string &sessionId, const std::string &joinToken, const std::string &role);

  StreamResult Read(uint8_t *buffer, size_t length) override;
  StreamResult WriteRaw(const uint8_t *buffer, size_t length) override;
  void Close() override;

private:
  using TcpLayer = boost::beast::tcp_stream;
  using SslLayer = boost::asio::ssl::stream<TcpLayer>;
  using WsStream = boost::beast::websocket::stream<TcpLayer>;
  using WssStream = boost::beast::websocket::stream<SslLayer>;
  using ErrorSlot = std::shared_ptr<boost::beast::error_code>;

  static ErrorSlot MakeSlot();

  bool RunUntilComplete(const ErrorSlot &slot);
  boost::beast::error_code Drive(const ErrorSlot &slot);
  void CloseSockets();
  boost::asio::ip::tcp::resolver::results_type Resolve(const std::string &host, const std::string &port);
  template <class Stream> void ConnectTo(Stream &stream, const boost::asio::ip::tcp::resolver::results_type &results);
  template <class Stream> void FinishHandshake(Stream &stream, const std::string &hostHeader, const std::string &target, const std::string &joinStr);
  template <class Stream> StreamResult ReadFrom(Stream &stream, uint8_t *out, size_t length);
  template <class Stream> StreamResult WriteTo(Stream &stream, const uint8_t *in, size_t length);

  static PacketError MapError(const boost::beast::error_code &ec, std::uint16_t closeCode);

  boost::asio::io_context m_Ctx{};
  std::unique_ptr<boost::asio::ip::tcp::resolver> m_Resolver{};
  std::unique_ptr<WssStream> m_Wss{}; // wss://
  std::unique_ptr<WsStream> m_Ws{};   // ws://
  boost::beast::flat_buffer m_ReadBuffer{};
  size_t m_ReadOffset{};
  std::atomic<bool> m_Closed{};
};

#endif // PCBU_DESKTOP_WEBSOCKETSTREAM_H
