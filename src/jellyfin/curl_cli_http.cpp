#include "jellyfin/curl_cli_http.h"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>

namespace zerofin::jf {
namespace {

// Value of a curl config file line: inside double quotes, with the escapes curl reads.
std::string config_quote(const std::string& s) {
  std::string q = "\"";
  for (char c : s) {
    switch (c) {
      case '\\': q += "\\\\"; break;
      case '"': q += "\\\""; break;
      case '\n': q += "\\n"; break;
      case '\r': q += "\\r"; break;
      case '\t': q += "\\t"; break;
      case '\v': q += "\\v"; break;
      default: q += c;
    }
  }
  return q + "\"";
}

// A private temp file (mkstemp makes it mode 0600). Removed when this object goes out of scope.
class TempFile {
 public:
  explicit TempFile(const std::string& content) {
    if (content.empty()) return;
    char path[] = "/tmp/zerofin_http_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) return;
    const bool ok = write(fd, content.data(), content.size()) == static_cast<ssize_t>(content.size());
    close(fd);
    if (ok) path_ = path; else unlink(path);
  }
  ~TempFile() {
    if (!path_.empty()) unlink(path_.c_str());
  }
  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;
  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

}  // namespace

HttpResponse CurlCliHttpClient::send(const HttpRequest& request) {
  // The URL (it can hold an api_key) and the headers (they hold the access token) go in a private
  // config file, not on the command line, where any local user could read them with ps.
  std::string config = "url = " + config_quote(request.url) + "\nrequest = " + config_quote(request.method) + "\n";
  for (const auto& h : request.headers) config += "header = " + config_quote(h.name + ": " + h.value) + "\n";
  const TempFile config_file(config);
  const TempFile body_file(request.body);
  HttpResponse res;
  if (config_file.path().empty() || (!request.body.empty() && body_file.path().empty())) {
    res.error = "cannot create a temporary file";
    return res;
  }

  std::string cmd = "/usr/bin/curl -sS -L --max-redirs 5 -m " + std::to_string(request.timeout_ms / 1000 + 1) +
                    " -K '" + config_file.path() + "'";
  if (!request.body.empty()) cmd += " --data-binary '@" + body_file.path() + "'";
  cmd += " -w '\n%{http_code}' 2>&1";

  std::string out;
  if (FILE* p = popen(cmd.c_str(), "r")) {
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);
  }

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
