#include "jellyfin/curl_http.h"

#include <curl/curl.h>

#include <memory>
#include <mutex>

namespace zerofin::jf {
namespace {

size_t write_body(char* data, size_t size, size_t count, void* user) {
  static_cast<std::string*>(user)->append(data, size * count);
  return size * count;
}

void global_init() {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

}  // namespace

CurlHttpClient::CurlHttpClient() { global_init(); }

HttpResponse CurlHttpClient::send(const HttpRequest& request) {
  HttpResponse response;
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
  if (!curl) {
    response.error = "cannot start libcurl";
    return response;
  }

  curl_slist* raw_headers = nullptr;
  for (const auto& h : request.headers) raw_headers = curl_slist_append(raw_headers, (h.name + ": " + h.value).c_str());
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(raw_headers, curl_slist_free_all);

  curl_easy_setopt(curl.get(), CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, request.method.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeout_ms));
  curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
  curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_body);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response.body);
  if (!request.body.empty()) {
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, request.body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
  } else if (request.method == "POST") {
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, "");
  }

  char error_buffer[CURL_ERROR_SIZE] = {0};
  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer);

  const CURLcode rc = curl_easy_perform(curl.get());
  if (rc != CURLE_OK) {
    response.status = 0;
    response.error = error_buffer[0] ? std::string(error_buffer) : std::string(curl_easy_strerror(rc));
    response.body.clear();
    return response;
  }
  long status = 0;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
  response.status = static_cast<int>(status);
  return response;
}

}  // namespace zerofin::jf
