#pragma once

#include "jellyfin/http.h"

namespace zerofin::jf {

// HttpClient backed by libcurl. TLS certificates are verified. Redirects are followed.
// Safe to call from one thread at a time. Use one instance per thread.
class CurlHttpClient : public HttpClient {
 public:
  CurlHttpClient();
  HttpResponse send(const HttpRequest& request) override;
};

}  // namespace zerofin::jf
