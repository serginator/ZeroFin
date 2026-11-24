#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zerofin::jf {

using QueryParams = std::vector<std::pair<std::string, std::string>>;

// RFC 3986 percent-encoding. Keeps unreserved characters.
std::string percent_encode(std::string_view text);

// Joins a base URL and a path with exactly one slash between them.
std::string join_url(std::string_view base, std::string_view path);

// Appends query parameters. Uses '?' or '&' as needed. Encodes keys and values.
std::string with_query(std::string_view url, const QueryParams& params);

// Sets one query parameter. Replaces an existing key (case-insensitive) or appends it.
// The value is encoded.
std::string set_query_param(std::string_view url, std::string_view key, std::string_view value);

// Returns the decoded value of a query parameter (case-insensitive key).
std::optional<std::string> get_query_param(std::string_view url, std::string_view key);

// Hides secrets (api_key, ApiKey, Token) so a URL is safe to log.
std::string redact_url(std::string_view url);

// Hides the value of api_key, ApiKey and token parameters anywhere in free text (a log line
// can hold a URL). The value ends at '&', a space, a quote or ')'.
std::string redact_text(std::string_view text);

// Cleans a server address typed by the user. Adds "http://" when there is no scheme.
// Removes spaces at both ends and trailing slashes. Returns nullopt for unusable input.
std::optional<std::string> normalize_server_url(std::string_view input);

}  // namespace zerofin::jf
