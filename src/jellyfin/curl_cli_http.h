#pragma once

#include <string>

#include "jellyfin/http.h"

namespace zerofin::jf {

// Development transport that runs /usr/bin/curl. macOS blocks LAN access for programs built
// on the machine (Local Network privacy), but allows Apple's own curl. Not for the device.
class CurlCliHttpClient : public HttpClient {
 public:
  HttpResponse send(const HttpRequest& request) override;
};

}  // namespace zerofin::jf
