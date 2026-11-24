#include "jellyfin/curl_cli_http.h"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>

namespace zerofin::jf {
namespace {

std::string quote(const std::string& s) {
  std::string q = "'";
  for (char c : s) q += (c == '\'') ? std::string("'\\''") : std::string(1, c);
  return q + "'";
}

}  // namespace

HttpResponse CurlCliHttpClient::send(const HttpRequest& request) {
  std::string cmd = "/usr/bin/curl -sS -m " + std::to_string(request.timeout_ms / 1000 + 1) + " -X " +
                    quote(request.method);
  for (const auto& h : request.headers) cmd += " -H " + quote(h.name + ": " + h.value);

  char body_path[] = "/tmp/zerofin_http_body_XXXXXX";
  int fd = -1;
  if (!request.body.empty()) {
    fd = mkstemp(body_path);
    if (fd >= 0) {
      (void)!write(fd, request.body.data(), request.body.size());
      close(fd);
      cmd += " --data-binary @" + std::string(body_path);
    }
  }
  cmd += " -w '\\n%{http_code}' " + quote(request.url) + " 2>&1";

  std::string out;
  if (FILE* p = popen(cmd.c_str(), "r")) {
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);
  }
  if (fd >= 0) unlink(body_path);

  HttpResponse res;
  const auto nl = out.rfind('\n');
  const int status = nl == std::string::npos ? 0 : std::atoi(out.c_str() + nl + 1);
  if (status == 0) {
    res.error = out.empty() ? "curl produced no output" : out;
  } else {
    res.status = status;
    res.body = out.substr(0, nl);
  }
  return res;
}

}  // namespace zerofin::jf
