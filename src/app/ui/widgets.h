#pragma once

#include <string>
#include <vector>

#include "lvgl.h"
#include "ui/theme.h"
#include "ui_core/keys.h"
#include "ui_core/list_cursor.h"
#include "ui_core/text_edit.h"

namespace zerofin::app {

lv_obj_t* make_plain_obj(lv_obj_t* parent, int x, int y, int w, int h);
lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, int x, int y, int w,
                     lv_label_long_mode_t mode = LV_LABEL_LONG_MODE_DOTS);

struct ListRow {
  std::string primary;
  std::string detail;     // right-aligned
  double progress = -1;   // 0..1 draws a thin bar under the row
  bool header = false;    // section title, cannot be selected
  bool dim = false;
  bool watched = false;   // adds a check mark to the detail
};

// A list that shows `visible` rows and scrolls. Only the visible rows exist as LVGL objects.
class ListView {
 public:
  ListView(lv_obj_t* parent, const Fonts& fonts, int x, int y, int width, size_t visible_rows);

  void set_rows(std::vector<ListRow> rows);
  // Replaces the rows with a centered message such as "Loading...".
  void set_message(const std::string& text, uint32_t color = color::kDim);

  // Up, Down, PageUp, PageDown, Home, End. Returns true when the key was a list key.
  bool handle_key(const ui::KeyEvent& event);

  int selected() const { return cursor_.selected(); }
  bool select(size_t index);
  bool near_end(size_t margin) const;
  void set_active(bool active);  // an inactive list draws no selection
  void set_detail_width(int width);  // width of the right-hand column, default 78

 private:
  struct RowObjs {
    lv_obj_t* row = nullptr;
    lv_obj_t* accent = nullptr;
    lv_obj_t* primary = nullptr;
    lv_obj_t* detail = nullptr;
    lv_obj_t* bar = nullptr;
  };
  void refresh();

  const Fonts& fonts_;
  int width_;
  size_t visible_;
  lv_obj_t* box_ = nullptr;
  lv_obj_t* message_ = nullptr;
  lv_obj_t* scroll_track_ = nullptr;
  lv_obj_t* scroll_thumb_ = nullptr;
  std::vector<RowObjs> objs_;
  std::vector<ListRow> rows_;
  ui::ListCursor cursor_;
  bool message_active_ = false;
  bool active_ = true;
  int detail_w_ = 78;
};

// One-line text box with a caret. The text model is ui::TextEdit.
class TextFieldView {
 public:
  TextFieldView(lv_obj_t* parent, const Fonts& fonts, int x, int y, int width, int height);

  bool handle_key(const ui::KeyEvent& event);  // true when the key edited or moved the caret
  const std::string& text() const { return edit_.text(); }
  void set_text(const std::string& text);
  void set_mask(bool mask);
  void set_placeholder(const std::string& text);
  void set_focused(bool focused);

 private:
  void render();

  const Fonts& fonts_;
  int width_;
  lv_obj_t* box_ = nullptr;
  lv_obj_t* label_ = nullptr;
  ui::TextEdit edit_;
  bool mask_ = false;
  bool focused_ = true;
  std::string placeholder_;
};

// A message bar at the bottom of the screen that hides itself.
class Toast {
 public:
  enum class Kind { Info, Success, Warning, Error };

  Toast(lv_obj_t* layer, const Fonts& fonts);
  ~Toast();
  void show(const std::string& text, Kind kind = Kind::Info, uint32_t ms = 2600);
  void hide();

 private:
  static void on_timer(lv_timer_t* timer);

  lv_obj_t* box_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_timer_t* timer_ = nullptr;
};

}  // namespace zerofin::app
