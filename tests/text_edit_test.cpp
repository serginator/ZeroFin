#include <doctest/doctest.h>

#include "ui_core/text_edit.h"

using namespace zerofin::ui;

namespace {
KeyEvent c(char ch) { return KeyEvent{Key::Char, ch, false}; }
KeyEvent k(Key key) { return KeyEvent{key, 0, false}; }
}  // namespace

TEST_CASE("typing appends at the cursor") {
  TextEdit t;
  for (char ch : std::string("abc")) CHECK(t.handle(c(ch)));
  CHECK(t.text() == "abc");
  CHECK(t.cursor() == 3);
}

TEST_CASE("typing in the middle inserts") {
  TextEdit t("ac");
  t.handle(k(Key::Left));
  t.handle(c('b'));
  CHECK(t.text() == "abc");
  CHECK(t.cursor() == 2);
}

TEST_CASE("backspace and delete") {
  TextEdit t("abc");
  t.handle(k(Key::Backspace));
  CHECK(t.text() == "ab");
  t.handle(k(Key::Home));
  t.handle(k(Key::Delete));
  CHECK(t.text() == "b");
  t.handle(k(Key::Backspace));  // at the start: nothing to remove
  CHECK(t.text() == "b");
}

TEST_CASE("cursor keys stop at both ends but still count as used") {
  TextEdit t("ab");
  CHECK(t.handle(k(Key::Right)));
  CHECK(t.cursor() == 2);
  t.handle(k(Key::Home));
  CHECK(t.handle(k(Key::Left)));
  CHECK(t.cursor() == 0);
  t.handle(k(Key::End));
  CHECK(t.cursor() == 2);
}

TEST_CASE("keys that are not for text are not used") {
  TextEdit t("ab");
  CHECK_FALSE(t.handle(k(Key::Enter)));
  CHECK_FALSE(t.handle(k(Key::Esc)));
  CHECK_FALSE(t.handle(k(Key::Up)));
  CHECK_FALSE(t.handle(k(Key::Tab)));
  CHECK_FALSE(t.handle(c('\n')));
  CHECK(t.text() == "ab");
}

TEST_CASE("the maximum length is enforced") {
  TextEdit t("", 3);
  for (char ch : std::string("abcdef")) t.handle(c(ch));
  CHECK(t.text() == "abc");
  TextEdit u("abcdef", 4);
  CHECK(u.text() == "abcd");
}

TEST_CASE("view shows everything when it fits") {
  TextEdit t("hello");
  auto v = t.view(10, false);
  CHECK(v.text == "hello");
  CHECK(v.caret == 5);
}

TEST_CASE("view scrolls so the caret stays visible") {
  TextEdit t("0123456789");
  auto v = t.view(5, false);
  CHECK(v.text == "6789");
  CHECK(v.caret == 4);
  t.handle(k(Key::Home));
  v = t.view(5, false);
  CHECK(v.text == "01234");
  CHECK(v.caret == 0);
}

TEST_CASE("view masks passwords") {
  TextEdit t("secret");
  CHECK(t.view(20, true).text == "******");
}

TEST_CASE("set_text and clear") {
  TextEdit t;
  t.set_text("http://");
  CHECK(t.cursor() == 7);
  t.clear();
  CHECK(t.empty());
  CHECK(t.cursor() == 0);
}
