#include "jellyfin/url.h"

#include <algorithm>
#include <cctype>

namespace zerofin::jf {
namespace {

bool is_unreserved(unsigned char c) {
  return std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

bool iequals(std::string_view a, std::string_view b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

std::string percent_decode(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
        std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
      out.push_back(static_cast<char>(std::stoi(std::string(text.substr(i + 1, 2)), nullptr, 16)));
      i += 2;
    } else if (text[i] == '+') {
      out.push_back(' ');
    } else {
      out.push_back(text[i]);
    }
  }
  return out;
}

struct Split {
  std::string base;      // everything before '?'
  std::string query;     // text after '?', without the '?'
  std::string fragment;  // text after '#', with the '#'
};

Split split_url(std::string_view url) {
  Split s;
  auto hash = url.find('#');
  if (hash != std::string_view::npos) {
    s.fragment = std::string(url.substr(hash));
    url = url.substr(0, hash);
  }
  auto q = url.find('?');
  if (q == std::string_view::npos) {
    s.base = std::string(url);
  } else {
    s.base = std::string(url.substr(0, q));
    s.query = std::string(url.substr(q + 1));
  }
  return s;
}

std::string join_split(const Split& s) {
  std::string out = s.base;
  if (!s.query.empty()) out += "?" + s.query;
  return out + s.fragment;
}

std::vector<std::string> split_pairs(const std::string& query) {
  std::vector<std::string> pairs;
  size_t start = 0;
  while (start <= query.size()) {
    auto amp = query.find('&', start);
    auto piece = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
    if (!piece.empty()) pairs.push_back(piece);
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  return pairs;
}

std::string pair_key(const std::string& pair) {
  return percent_decode(pair.substr(0, pair.find('=')));
}

}  // namespace

std::string percent_encode(std::string_view text) {
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : text) {
    if (is_unreserved(c)) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(kHex[c >> 4]);
      out.push_back(kHex[c & 0x0F]);
    }
  }
  return out;
}

std::string join_url(std::string_view base, std::string_view path) {
  while (!base.empty() && base.back() == '/') base.remove_suffix(1);
  while (!path.empty() && path.front() == '/') path.remove_prefix(1);
  return std::string(base) + "/" + std::string(path);
}

std::string with_query(std::string_view url, const QueryParams& params) {
  Split s = split_url(url);
  for (const auto& [k, v] : params) {
    if (!s.query.empty()) s.query += "&";
    s.query += percent_encode(k) + "=" + percent_encode(v);
  }
  return join_split(s);
}

std::string set_query_param(std::string_view url, std::string_view key, std::string_view value) {
  Split s = split_url(url);
  std::vector<std::string> pairs = split_pairs(s.query);
  const std::string replacement = percent_encode(key) + "=" + percent_encode(value);
  bool replaced = false;
  std::string rebuilt;
  for (const auto& pair : pairs) {
    if (iequals(pair_key(pair), key)) {
      if (replaced) continue;  // drop duplicates of the same key
      replaced = true;
      rebuilt += (rebuilt.empty() ? "" : "&") + replacement;
    } else {
      rebuilt += (rebuilt.empty() ? "" : "&") + pair;
    }
  }
  if (!replaced) rebuilt += (rebuilt.empty() ? "" : "&") + replacement;
  s.query = rebuilt;
  return join_split(s);
}

std::optional<std::string> get_query_param(std::string_view url, std::string_view key) {
  Split s = split_url(url);
  for (const auto& pair : split_pairs(s.query)) {
    if (iequals(pair_key(pair), key)) {
      auto eq = pair.find('=');
      return eq == std::string::npos ? std::string() : percent_decode(pair.substr(eq + 1));
    }
  }
  return std::nullopt;
}

std::string redact_url(std::string_view url) {
  Split s = split_url(url);
  std::string rebuilt;
  for (const auto& pair : split_pairs(s.query)) {
    const std::string key = pair_key(pair);
    const bool secret = iequals(key, "api_key") || iequals(key, "apikey") || iequals(key, "token") ||
                        iequals(key, "x-emby-token");
    rebuilt += (rebuilt.empty() ? "" : "&") + (secret ? pair.substr(0, pair.find('=')) + "=***" : pair);
  }
  s.query = rebuilt;
  return join_split(s);
}

std::string redact_text(std::string_view text) {
  static const char* const kKeys[] = {"api_key=", "apikey=", "token="};
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    size_t matched = 0;
    for (const char* key : kKeys) {
      const size_t n = std::char_traits<char>::length(key);
      if (i + n > text.size()) continue;
      bool same = true;
      for (size_t k = 0; k < n && same; ++k) {
        same = std::tolower(static_cast<unsigned char>(text[i + k])) == key[k];
      }
      if (same) {
        matched = n;
        break;
      }
    }
    if (matched == 0) {
      out += text[i++];
      continue;
    }
    out.append(text.substr(i, matched));
    out += "***";
    i += matched;
    while (i < text.size()) {
      const char c = text[i];
      if (c == '&' || c == ' ' || c == '"' || c == '\'' || c == ')' || c == '\n' || c == '\t') break;
      ++i;
    }
  }
  return out;
}

std::optional<std::string> normalize_server_url(std::string_view input) {
  while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
  while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) input.remove_suffix(1);
  if (input.empty()) return std::nullopt;
  if (std::any_of(input.begin(), input.end(), [](unsigned char c) { return std::isspace(c) != 0; })) {
    return std::nullopt;
  }
  std::string url(input);
  const auto scheme_end = url.find("://");
  if (scheme_end == std::string::npos) {
    url = "http://" + url;
  } else {
    std::string scheme = url.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](unsigned char c) { return std::tolower(c); });
    if (scheme != "http" && scheme != "https") return std::nullopt;
    url = scheme + url.substr(scheme_end);
  }
  const auto host_start = url.find("://") + 3;
  while (url.size() > host_start && url.back() == '/') url.pop_back();
  if (host_start >= url.size() || url[host_start] == ':' || url[host_start] == '/') return std::nullopt;
  return url;
}

}  // namespace zerofin::jf
