#pragma once

#include "jellyfin/http.h"

namespace zerofin::jf {

// A fake Jellyfin server in memory, for the simulator and for UI work without a network.
// Enable it with ZEROFIN_DEMO=1. Login: Quick Connect (approves after a few polls) or
// the user "demo" with the password "demo".
class DemoHttpClient : public HttpClient {
 public:
  DemoHttpClient();
  HttpResponse send(const HttpRequest& request) override;

 private:
  int quick_connect_polls_ = 0;
  int latency_ms_ = 150;
  int approve_after_polls_ = 3;
};

}  // namespace zerofin::jf
