#include "HttpClient.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <boost/asio/ip/address.hpp>
#include <boost/optional.hpp>
#include <boost/url.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

#include "connection/web/TlsContext.h"
#include "utils/AppInfo.h"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace urls = boost::urls;

std::string HttpResponse::GetHeader(const std::string &name) const {
  for(const auto &[key, value] : headers) {
    if(key.size() != name.size())
      continue;
    if(std::equal(key.begin(), key.end(), name.begin(),
                  [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); }))
      return value;
  }
  return {};
}

HttpClient::~HttpClient() {
  CloseStreams();
}

HttpResponse HttpClient::Get(const std::string &url, const HttpOptions &options) {
  return Send(http::verb::get, url, {}, {}, options, nullptr);
}

HttpResponse HttpClient::Post(const std::string &url, const std::string &body, const std::string &contentType, const HttpOptions &options) {
  return Send(http::verb::post, url, body, contentType, options, nullptr);
}

HttpResponse HttpClient::Download(const std::string &url, const DownloadHandler &onData, const ProgressHandler &onProgress,
                                  const HttpOptions &options) {
  if(!onData)
    return {.ok = false, .error = "No download handler given."};
  const DownloadSink sink = {.onData = &onData, .onProgress = &onProgress};
  return Send(http::verb::get, url, {}, {}, options, &sink);
}

void HttpClient::Cancel() {
  if(m_Canceled.exchange(true))
    return;
  net::post(m_Ctx, [this] { CloseStreams(); });
}

void HttpClient::CloseStreams() {
  if(m_Resolver)
    m_Resolver->cancel();
  if(m_Ssl)
    beast::get_lowest_layer(*m_Ssl).close();
  if(m_Plain)
    m_Plain->close();
}

void HttpClient::Run() {
  m_Ctx.restart();
  m_Ctx.run();
}

template <class Parser> void HttpClient::ApplyBodyLimit(Parser &parser, const HttpOptions &options) {
  if(options.bodyLimit > 0)
    parser.body_limit(options.bodyLimit);
  else
    parser.body_limit(boost::none);
}

template <class Message> void HttpClient::CollectHeaders(const Message &message, HttpResponse &response) {
  for(const auto &field : message)
    response.headers.emplace_back(std::string(field.name_string()), std::string(field.value()));
}

bool HttpClient::IsRedirect(int status) {
  return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

bool HttpClient::ParseUrl(const std::string &url, Endpoint &endpoint) {
  auto parsed = urls::parse_uri(url);
  if(!parsed)
    return false;
  const urls::url_view view = *parsed;
  const auto scheme = view.scheme_id();
  if(scheme != urls::scheme::http && scheme != urls::scheme::https)
    return false;

  endpoint.secure = scheme == urls::scheme::https;
  endpoint.host = view.host();
  if(endpoint.host.empty())
    return false;
  endpoint.port = view.has_port() ? std::string(view.port()) : std::string(endpoint.secure ? "443" : "80");
  endpoint.target = view.encoded_target().empty() ? std::string("/") : std::string(view.encoded_target());
  endpoint.hostHeader = std::string(view.encoded_host_and_port());
  return true;
}

std::string HttpClient::ResolveRedirect(const std::string &baseUrl, const std::string &location) {
  auto base = urls::parse_uri(baseUrl);
  auto ref = urls::parse_uri_reference(location);
  if(!base || !ref)
    return {};
  urls::url resolved{*base};
  if(!resolved.resolve(*ref))
    return {};
  return std::string(resolved.buffer());
}

HttpResponse HttpClient::Send(http::verb method, const std::string &url, const std::string &body, const std::string &contentType,
                              const HttpOptions &options, const DownloadSink *sink) {
  auto currentUrl = url;
  for(auto hop = 0; hop <= options.maxRedirects; ++hop) {
    if(m_Canceled)
      return {.ok = false, .error = "Canceled."};

    Endpoint endpoint{};
    if(!ParseUrl(currentUrl, endpoint))
      return {.ok = false, .error = "Invalid URL."};

    auto response = SendOnce(endpoint, method, body, contentType, options, sink);
    if(!response.ok || hop >= options.maxRedirects || !IsRedirect(response.status))
      return response;

    auto location = response.GetHeader("location");
    if(location.empty())
      return response;
    auto nextUrl = ResolveRedirect(currentUrl, location);
    Endpoint nextEndpoint{};
    if(nextUrl.empty() || !ParseUrl(nextUrl, nextEndpoint)) {
      response.ok = false;
      response.error = "Invalid redirect target.";
      return response;
    }
    if(endpoint.secure && !nextEndpoint.secure) {
      response.ok = false;
      response.error = "Refusing to follow a redirect from HTTPS to HTTP.";
      return response;
    }
    spdlog::debug("Following HTTP redirect to '{}'.", nextUrl);
    currentUrl = nextUrl;
  }
  return {.ok = false, .error = "Too many redirects."};
}

HttpResponse HttpClient::SendOnce(const Endpoint &endpoint, http::verb method, const std::string &body, const std::string &contentType,
                                  const HttpOptions &options, const DownloadSink *sink) {
  m_Plain.reset();
  m_Ssl.reset();
  try {
    auto results = Resolve(endpoint, options);
    if(m_Canceled)
      throw std::runtime_error("Canceled.");

    beast::error_code ec = net::error::would_block;
    if(endpoint.secure) {
      m_Ssl = std::make_unique<SslStream>(m_Ctx, TlsContext::Shared());
      auto addressEc = beast::error_code();
      net::ip::make_address(endpoint.host, addressEc);
      if(addressEc && !SSL_set_tlsext_host_name(m_Ssl->native_handle(), endpoint.host.c_str())) {
        ERR_clear_error();
        throw std::runtime_error("Failed to set TLS SNI host name.");
      }
      m_Ssl->set_verify_mode(ssl::verify_peer);
      m_Ssl->set_verify_callback(ssl::host_name_verification(endpoint.host));

      beast::get_lowest_layer(*m_Ssl).expires_after(options.connectTimeout);
      beast::get_lowest_layer(*m_Ssl).async_connect(results, [&ec](beast::error_code e, const net::ip::tcp::endpoint &) { ec = e; });
      Run();
      if(ec)
        throw std::runtime_error("Connect failed: " + ec.message());

      beast::get_lowest_layer(*m_Ssl).expires_after(options.connectTimeout);
      m_Ssl->async_handshake(ssl::stream_base::client, [&ec](beast::error_code e) { ec = e; });
      Run();
      if(ec)
        throw std::runtime_error("TLS handshake failed: " + ec.message());

      return Exchange(*m_Ssl, endpoint, method, body, contentType, options, sink);
    }

    m_Plain = std::make_unique<PlainStream>(m_Ctx);
    m_Plain->expires_after(options.connectTimeout);
    m_Plain->async_connect(results, [&ec](beast::error_code e, const net::ip::tcp::endpoint &) { ec = e; });
    Run();
    if(ec)
      throw std::runtime_error("Connect failed: " + ec.message());

    return Exchange(*m_Plain, endpoint, method, body, contentType, options, sink);
  } catch(const std::exception &ex) {
    CloseStreams();
    if(m_Canceled) {
      spdlog::debug("HTTP request canceled.");
      return {.ok = false, .error = "Canceled."};
    }
    spdlog::error("HTTP request to '{}' failed: {}", endpoint.hostHeader, ex.what());
    return {.ok = false, .error = ex.what()};
  }
}

template <class Stream>
HttpResponse HttpClient::Exchange(Stream &stream, const Endpoint &endpoint, http::verb method, const std::string &body,
                                  const std::string &contentType, const HttpOptions &options, const DownloadSink *sink) {
  http::request<http::string_body> request{method, endpoint.target, 11};
  request.set(http::field::host, endpoint.hostHeader);
  request.set(http::field::user_agent, "pcbu-desktop " + AppInfo::GetVersion());
  request.set(http::field::accept, "*/*");
  request.set(http::field::accept_encoding, "identity");
  request.set(http::field::connection, "close");
  if(!contentType.empty())
    request.set(http::field::content_type, contentType);
  request.body() = body;
  request.prepare_payload();

  beast::error_code ec = net::error::would_block;
  beast::get_lowest_layer(stream).expires_after(options.transferTimeout);
  http::async_write(stream, request, [&ec](beast::error_code e, std::size_t) { ec = e; });
  Run();
  if(ec)
    throw std::runtime_error("Sending request failed: " + ec.message());

  beast::flat_buffer buffer{};
  auto result = sink == nullptr ? ReadBuffered(stream, buffer, options) : ReadStreamed(stream, buffer, options, *sink);
  CloseStreams();
  return result;
}

template <class Stream> HttpResponse HttpClient::ReadBuffered(Stream &stream, beast::flat_buffer &buffer, const HttpOptions &options) {
  http::response_parser<http::string_body> parser{};
  ApplyBodyLimit(parser, options);

  beast::error_code ec = net::error::would_block;
  beast::get_lowest_layer(stream).expires_after(options.transferTimeout);
  http::async_read(stream, buffer, parser, [&ec](beast::error_code e, std::size_t) { ec = e; });
  Run();
  if(ec && !(parser.is_done() && (ec == net::error::eof || ec == ssl::error::stream_truncated)))
    throw std::runtime_error("Reading response failed: " + ec.message());

  auto &message = parser.get();
  auto result = HttpResponse();
  result.ok = true;
  result.status = static_cast<int>(message.result_int());
  result.body = std::move(message.body());
  CollectHeaders(message, result);
  return result;
}

template <class Stream>
HttpResponse HttpClient::ReadStreamed(Stream &stream, beast::flat_buffer &buffer, const HttpOptions &options, const DownloadSink &sink) {
  http::response_parser<http::buffer_body> parser{};
  ApplyBodyLimit(parser, options);

  beast::error_code ec = net::error::would_block;
  beast::get_lowest_layer(stream).expires_after(options.transferTimeout);
  http::async_read_header(stream, buffer, parser, [&ec](beast::error_code e, std::size_t) { ec = e; });
  Run();
  if(ec)
    throw std::runtime_error("Reading response header failed: " + ec.message());

  auto &message = parser.get();
  auto result = HttpResponse();
  result.ok = true;
  result.status = static_cast<int>(message.result_int());
  CollectHeaders(message, result);

  const auto isRedirect = IsRedirect(result.status);
  const auto total = parser.content_length().value_or(0);
  std::uint64_t transferred = 0;
  std::vector<char> chunk(64 * 1024);
  while(!parser.is_done()) {
    message.body().data = chunk.data();
    message.body().size = chunk.size();
    ec = net::error::would_block;
    beast::get_lowest_layer(stream).expires_after(options.transferTimeout);
    http::async_read_some(stream, buffer, parser, [&ec](beast::error_code e, std::size_t) { ec = e; });
    Run();
    if(ec == http::error::need_buffer)
      ec = {};
    if(ec && !(parser.is_done() && (ec == net::error::eof || ec == ssl::error::stream_truncated)))
      throw std::runtime_error("Reading response failed: " + ec.message());

    const auto received = chunk.size() - message.body().size;
    if(received == 0 || isRedirect)
      continue;
    transferred += received;
    if(!(*sink.onData)(reinterpret_cast<const std::uint8_t *>(chunk.data()), received))
      throw std::runtime_error("Download aborted.");
    if(*sink.onProgress && !(*sink.onProgress)(transferred, total))
      throw std::runtime_error("Download aborted.");
  }
  return result;
}

net::ip::tcp::resolver::results_type HttpClient::Resolve(const Endpoint &endpoint, const HttpOptions &options) {
  m_Resolver = std::make_unique<net::ip::tcp::resolver>(m_Ctx);
  beast::error_code ec = net::error::would_block;
  net::ip::tcp::resolver::results_type results{};
  m_Resolver->async_resolve(endpoint.host, endpoint.port, [&](beast::error_code e, const net::ip::tcp::resolver::results_type &r) {
    ec = e;
    results = r;
  });

  m_Ctx.restart();
  m_Ctx.run_for(options.connectTimeout);
  if(!m_Ctx.stopped()) {
    m_Resolver->cancel();
    m_Ctx.run();
  }
  m_Resolver.reset();

  if(ec == net::error::would_block)
    throw std::runtime_error("Host lookup did not complete.");
  if(ec)
    throw std::runtime_error("Host lookup failed: " + ec.message());
  return results;
}
