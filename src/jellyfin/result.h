#pragma once

#include <string>
#include <utility>
#include <variant>

namespace zerofin::jf {

enum class ErrorKind { Network, Http, Unauthorized, NotFound, Parse, Invalid };

struct Error {
  ErrorKind kind = ErrorKind::Invalid;
  std::string message;
  int http_status = 0;
};

struct Unit {};

// A value or an Error. C++17 has no std::expected.
template <typename T>
class Result {
 public:
  Result(T value) : v_(std::move(value)) {}
  Result(Error error) : v_(std::move(error)) {}

  bool ok() const { return std::holds_alternative<T>(v_); }
  explicit operator bool() const { return ok(); }
  const T& value() const { return std::get<T>(v_); }
  T& value() { return std::get<T>(v_); }
  const Error& error() const { return std::get<Error>(v_); }

 private:
  std::variant<T, Error> v_;
};

}  // namespace zerofin::jf
