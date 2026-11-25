#include <doctest/doctest.h>

#include "ui_core/request_queue.h"

using namespace zerofin::ui;

TEST_CASE("lower priority number first, then caller order") {
  RequestQueue q;
  q.set_wanted({{"c", 2}, {"a", 0}, {"b", 0}, {"d", 1}});
  CHECK(q.next() == std::optional<std::string>("a"));
  q.done("a");
  CHECK(q.next() == std::optional<std::string>("b"));
  q.done("b");
  CHECK(q.next() == std::optional<std::string>("d"));
  q.done("d");
  CHECK(q.next() == std::optional<std::string>("c"));
}

TEST_CASE("only one request in flight") {
  RequestQueue q;
  q.set_wanted({{"a", 0}, {"b", 0}});
  CHECK(q.next().has_value());
  CHECK(q.in_flight());
  CHECK_FALSE(q.next().has_value());
}

TEST_CASE("set_wanted drops entries that are no longer wanted") {
  RequestQueue q;
  q.set_wanted({{"a", 0}, {"b", 0}});
  q.set_wanted({{"b", 0}});
  CHECK(q.next() == std::optional<std::string>("b"));
  q.done("b");
  CHECK_FALSE(q.next().has_value());
}

TEST_CASE("the in-flight key is not queued again") {
  RequestQueue q;
  q.set_wanted({{"a", 0}});
  REQUIRE(q.next().has_value());
  q.set_wanted({{"a", 0}, {"b", 1}});
  q.done("a");
  CHECK(q.next() == std::optional<std::string>("b"));
}

TEST_CASE("done lets the next request start") {
  RequestQueue q;
  q.set_wanted({{"a", 0}, {"b", 0}});
  q.next();
  q.done("a");
  CHECK_FALSE(q.in_flight());
  CHECK(q.next() == std::optional<std::string>("b"));
}
