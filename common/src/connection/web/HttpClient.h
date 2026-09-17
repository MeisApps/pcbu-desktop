#ifndef PCBU_DESKTOP_HTTPCLIENT_H
#define PCBU_DESKTOP_HTTPCLIENT_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

struct HttpOptions {
  std::chrono::seconds connectTimeout{5};
  std::chrono::seconds transferTimeout{10};
  std::size_t bodyLimit{1024 * 1024};
  int maxRedirects{};
};

struct HttpResponse {
  bool ok{};
  int status{};
  std::string body{};
  std::string error{};
  std::vector<std::pair<std::string, std::string>> headers{};

  [[nodiscard]] std::string GetHeader(const std::string &name) const;
};

class HttpClient {
public:
  HttpClient() = default;
  ~HttpClient();

  HttpClient(const HttpClient &) = delete;
  HttpClient &operator=(const HttpClient &) = delete;

  using DownloadHandler = std::function<bool(const std::uint8_t *data, std::size_t length)>;
  using ProgressHandler = std::function<bool(std::uint64_t transferred, std::uint64_t total)>;

  HttpResponse Get(const std::string &url, const HttpOptions &options = {});
  HttpResponse Post(const std::string &url, const std::string &body, const std::string &contentType, const HttpOptions &options = {});

  HttpResponse Download(const std::string &url, const DownloadHandler &onData, const ProgressHandler &onProgress, const HttpOptions &options = {});

  void Cancel();

private:
  struct Endpoint {
    std::string host{};
    std::string port{};
    std::string target{};
    std::string hostHeader{};
    bool secure{};
  };

  struct DownloadSink {
    const DownloadHandler *onData{};
    const ProgressHandler *onProgress{};
  };

  using PlainStream = boost::beast::tcp_stream;
  using SslStream = boost::asio::ssl::stream<boost::beast::tcp_stream>;

  static bool ParseUrl(const std::string &url, Endpoint &endpoint);
  static bool IsRedirect(int status);
  static std::string ResolveRedirect(const std::string &baseUrl, const std::string &location);

  HttpResponse Send(boost::beast::http::verb method, const std::string &url, const std::string &body, const std::string &contentType,
                    const HttpOptions &options, const DownloadSink *sink);
  HttpResponse SendOnce(const Endpoint &endpoint, boost::beast::http::verb method, const std::string &body, const std::string &contentType,
                        const HttpOptions &options, const DownloadSink *sink);
  template <class Stream>
  HttpResponse Exchange(Stream &stream, const Endpoint &endpoint, boost::beast::http::verb method, const std::string &body,
                        const std::string &contentType, const HttpOptions &options, const DownloadSink *sink);
  template <class Stream> HttpResponse ReadBuffered(Stream &stream, boost::beast::flat_buffer &buffer, const HttpOptions &options);
  template <class Stream>
  HttpResponse ReadStreamed(Stream &stream, boost::beast::flat_buffer &buffer, const HttpOptions &options, const DownloadSink &sink);

  template <class Parser> static void ApplyBodyLimit(Parser &parser, const HttpOptions &options);
  template <class Message> static void CollectHeaders(const Message &message, HttpResponse &response);

  boost::asio::ip::tcp::resolver::results_type Resolve(const Endpoint &endpoint, const HttpOptions &options);
  void CloseStreams();
  void Run();

  boost::asio::io_context m_Ctx{};
  std::unique_ptr<boost::asio::ip::tcp::resolver> m_Resolver{};
  std::unique_ptr<PlainStream> m_Plain{};
  std::unique_ptr<SslStream> m_Ssl{};
  std::atomic<bool> m_Canceled{};
};

#endif // PCBU_DESKTOP_HTTPCLIENT_H
