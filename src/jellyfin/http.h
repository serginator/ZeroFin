#pragma once

#include <string>
#include <vector>

namespace zerofin::jf {

struct Header {
  std::string name;
  std::string value;
};

struct HttpRequest {
  std::string method = "GET";
  std::string url;
  std::vector<Header> headers;
  std::string body;
  int timeout_ms = 10000;
};

struct HttpResponse {
  int status = 0;       // 0 when the transport failed
  std::string body;
  std::string error;    // transport error text, empty when a response arrived
};

// Transport seam. The real one uses libcurl. Tests use a fake.
class HttpClient {
 public:
  virtual ~HttpClient() = default;
  virtual HttpResponse send(const HttpRequest& request) = 0;
};

}  // namespace zerofin::jf
