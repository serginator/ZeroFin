#include "ui/poster_widgets.h"

#include <algorithm>
#include <cctype>

#include "ui/widgets.h"

namespace zerofin::app {
namespace {

constexpr int kCaptionW = 180;
constexpr int kCaptionMargin = 4;

void set_visible(lv_obj_t* o, bool on) { lv_obj_set_flag(o, LV_OBJ_FLAG_HIDDEN, !on); }

uint32_t mix(uint32_t a, uint32_t b, int pct_a) {
  auto ch = [&](int shift) {
    const uint32_t x = (a >> shift) & 0xFF;
    const uint32_t y = (b >> shift) & 0xFF;
    return ((x * static_cast<uint32_t>(pct_a) + y * static_cast<uint32_t>(100 - pct_a)) / 100) << shift;
  };
  return ch(16) | ch(8) | ch(0);
}

// The first character of a title as UTF-8, in upper case when it is ASCII.
std::string initial(const std::string& s) {
  if (s.empty()) return "?";
  const unsigned char c = static_cast<unsigned char>(s[0]);
  const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
  std::string out = s.substr(0, std::min(len, s.size()));
  if (len == 1) out[0] = static_cast<char>(std::toupper(c));
  return out;
}

lv_obj_t* make_box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg, int radius) {
  lv_obj_t* o = make_plain_obj(parent, x, y, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, radius, 0);
  return o;
}

}  // namespace

// --------------------------------------------------------------- PosterCardView

PosterCardView::PosterCardView(lv_obj_t* row, const Fonts& fonts) {
  frame_ = make_box(row, 0, 0, kCardW, kCardH, color::kPlaceholder, 3);
  lv_obj_set_style_clip_corner(frame_, true, 0);
  lv_obj_set_style_border_color(frame_, lv_color_hex(color::kSelectedBorder), 0);
  lv_obj_set_style_border_post(frame_, true, 0);

  letter_ = make_label(frame_, fonts.bold, color::kDim, 0, (kCardH - lv_font_get_line_height(fonts.bold)) / 2, kCardW,
                       LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(letter_, LV_TEXT_ALIGN_CENTER, 0);

  img_ = lv_image_create(frame_);
  lv_obj_remove_flag(img_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(img_, LV_OBJ_FLAG_HIDDEN);

  badge_ = lv_label_create(frame_);
  lv_obj_set_style_text_font(badge_, fonts.small, 0);
  lv_obj_set_style_text_color(badge_, lv_color_hex(color::kText), 0);
  lv_obj_set_style_bg_color(badge_, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(badge_, LV_OPA_70, 0);
  lv_obj_set_style_pad_hor(badge_, 3, 0);
  lv_obj_set_style_radius(badge_, 2, 0);
  lv_obj_set_pos(badge_, 3, 3);

  check_ = lv_label_create(frame_);
  lv_obj_set_style_text_font(check_, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(check_, lv_color_hex(color::kGood), 0);
  lv_obj_set_style_bg_color(check_, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(check_, LV_OPA_70, 0);
  lv_obj_set_style_pad_hor(check_, 2, 0);
  lv_obj_set_style_radius(check_, 2, 0);
  lv_label_set_text(check_, LV_SYMBOL_OK);
  lv_obj_align(check_, LV_ALIGN_TOP_RIGHT, -3, 3);

  bar_ = make_box(frame_, 2, kCardH - 7, kCardW - 4, 4, 0x000000, 0);
  lv_obj_set_style_bg_opa(bar_, LV_OPA_60, 0);
  bar_fill_ = make_box(bar_, 0, 0, 0, 4, color::kAccent, 0);

  caption_ = make_label(row, fonts.normal, color::kText, 0, 0, kCaptionW);
  lv_obj_add_flag(frame_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(caption_, LV_OBJ_FLAG_HIDDEN);
}

void PosterCardView::set_pos(int x, int y) {
  lv_obj_set_pos(frame_, x, y);
  const int cx = std::clamp(x, kCaptionMargin, kScreenW - kCaptionMargin - kCaptionW);
  lv_obj_set_pos(caption_, cx, y + kCardH + 1);
}

void PosterCardView::bind(const CardModel* model, ImageLoader::PosterPtr poster, bool failed) {
  bound_ = model != nullptr;
  if (!model) {
    set_visible(frame_, false);
    set_visible(caption_, false);
    poster_.reset();
    return;
  }
  set_visible(frame_, true);
  if (poster != poster_) {
    poster_ = std::move(poster);
    if (poster_) {
      lv_image_set_src(img_, &poster_->dsc);
      lv_obj_center(img_);
    }
  }
  set_visible(img_, poster_ != nullptr);
  set_visible(letter_, poster_ == nullptr);
  if (!poster_) {
    lv_label_set_text(letter_, initial(model->text.caption).c_str());
    lv_obj_set_style_text_color(letter_, lv_color_hex(failed ? color::kPlaceholder + 0x181818 : color::kDim), 0);
  }
  lv_label_set_text(caption_, model->text.caption.c_str());
  set_visible(badge_, !model->text.badge.empty());
  if (!model->text.badge.empty()) lv_label_set_text(badge_, model->text.badge.c_str());
  set_visible(check_, model->text.watched);
  const bool progress = model->text.progress > 0;
  set_visible(bar_, progress);
  if (progress) lv_obj_set_width(bar_fill_, static_cast<int>(model->text.progress * (kCardW - 4)));
}

void PosterCardView::set_selected(bool selected) {
  lv_obj_set_style_border_width(frame_, selected ? 2 : 0, 0);
  set_visible(caption_, selected && bound_);
}

// --------------------------------------------------------------------- TileView

TileView::TileView(lv_obj_t* row, const Fonts& fonts) {
  back_ = make_box(row, 0, 0, 36, 36, color::kBar, 5);
  mid_ = make_box(row, 0, 0, 36, 36, color::kBar, 5);
  front_ = make_box(row, 0, 0, 36, 36, color::kAccent, 5);
  lv_obj_set_style_border_color(front_, lv_color_hex(color::kSelectedBorder), 0);
  glyph_ = make_label(front_, &lv_font_montserrat_16, color::kText, 0, 9, 36, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(glyph_, LV_TEXT_ALIGN_CENTER, 0);

  // A magnifier for Search: a ring and a handle turned by 45 degrees.
  lens_ = make_plain_obj(front_, 8, 7, 14, 14);
  lv_obj_set_style_radius(lens_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(lens_, 2, 0);
  lv_obj_set_style_border_color(lens_, lv_color_hex(color::kText), 0);
  handle_ = make_box(front_, 22, 20, 3, 9, color::kText, 1);
  lv_obj_set_style_transform_pivot_x(handle_, 1, 0);
  lv_obj_set_style_transform_pivot_y(handle_, 4, 0);
  lv_obj_set_style_transform_rotation(handle_, 3150, 0);

  name_ = make_label(row, fonts.small, color::kDim, 0, 0, kTileStep);
  lv_obj_set_style_text_align(name_, LV_TEXT_ALIGN_CENTER, 0);
  for (lv_obj_t* o : {back_, mid_, front_, name_}) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void TileView::set_pos(int x, int y) {
  lv_obj_set_pos(back_, x + 8, y);
  lv_obj_set_pos(mid_, x + 4, y + 4);
  lv_obj_set_pos(front_, x, y + 8);
  lv_obj_set_pos(name_, x + 22 - kTileStep / 2, y + 46);
}

void TileView::bind(const TileModel* model) {
  for (lv_obj_t* o : {back_, mid_, front_, name_}) set_visible(o, model != nullptr);
  if (!model) return;
  lv_obj_set_style_bg_color(back_, lv_color_hex(mix(model->tint, color::kBackground, 35)), 0);
  lv_obj_set_style_bg_color(mid_, lv_color_hex(mix(model->tint, color::kBackground, 60)), 0);
  lv_obj_set_style_bg_color(front_, lv_color_hex(model->tint), 0);
  const bool search = model->kind == ui::TileKind::Search;
  set_visible(lens_, search);
  set_visible(handle_, search);
  set_visible(glyph_, !search);
  switch (model->kind) {
    case ui::TileKind::Movies: lv_label_set_text(glyph_, LV_SYMBOL_VIDEO); break;
    case ui::TileKind::TvShows: lv_label_set_text(glyph_, LV_SYMBOL_LIST); break;
    case ui::TileKind::Collections: lv_label_set_text(glyph_, LV_SYMBOL_DIRECTORY); break;
    case ui::TileKind::Settings: lv_label_set_text(glyph_, LV_SYMBOL_SETTINGS); break;
    default: lv_label_set_text(glyph_, LV_SYMBOL_IMAGE); break;
  }
  lv_label_set_text(name_, model->label.c_str());
}

void TileView::set_selected(bool selected) {
  lv_obj_set_style_border_width(front_, selected ? 2 : 0, 0);
  lv_obj_set_style_text_color(name_, lv_color_hex(selected ? color::kText : color::kDim), 0);
}

// ------------------------------------------------------------------ CardRowView

CardRowView::CardRowView(lv_obj_t* parent, const Fonts& fonts, Kind kind, size_t pool, int step)
    : kind_(kind), step_(step) {
  box_ = make_plain_obj(parent, 0, 0, kScreenW, 1);
  header_ = make_label(box_, fonts.bold, color::kText, kRowMargin, 0, kScreenW - 2 * kRowMargin);
  lv_obj_add_flag(header_, LV_OBJ_FLAG_HIDDEN);
  for (size_t i = 0; i < pool; ++i) {
    if (kind_ == Kind::Tiles) tile_views_.push_back(std::make_unique<TileView>(box_, fonts));
    else card_views_.push_back(std::make_unique<PosterCardView>(box_, fonts));
  }
  lv_obj_set_height(box_, height());
}

CardRowView::~CardRowView() {
  // The card views and the tile views hold only pointers to children of box_. Deleting box_ first means
  // their poster pointers are released after LVGL stopped showing the pictures.
  if (box_) lv_obj_delete(box_);
}

void CardRowView::set_header(const std::string& text) {
  header_h_ = text.empty() ? 0 : kHeaderH;
  lv_label_set_text(header_, text.c_str());
  set_visible(header_, !text.empty());
  lv_obj_set_height(box_, height());
}

int CardRowView::height() const {
  if (kind_ == Kind::Tiles) return kTileBlockH;
  return header_h_ + kCardH + kCaptionH + (header_h_ ? 0 : 2);
}

void CardRowView::set_y(int y) { lv_obj_set_y(box_, y); }
void CardRowView::set_cards(std::vector<CardModel> cards) { cards_ = std::move(cards); }
void CardRowView::set_tiles(std::vector<TileModel> tiles) { tiles_ = std::move(tiles); }

void CardRowView::show(ImageLoader& images, size_t left, int selected_col) {
  lv_obj_remove_flag(box_, LV_OBJ_FLAG_HIDDEN);
  if (kind_ == Kind::Tiles) {
    for (size_t i = 0; i < tile_views_.size(); ++i) {
      const size_t col = left + i;
      TileView& v = *tile_views_[i];
      if (col >= tiles_.size()) { v.bind(nullptr); continue; }
      v.set_pos(12 + static_cast<int>(i) * step_, 0);
      v.bind(&tiles_[col]);
      v.set_selected(static_cast<int>(col) == selected_col);
    }
    return;
  }
  for (size_t i = 0; i < card_views_.size(); ++i) {
    const size_t col = left + i;
    PosterCardView& v = *card_views_[i];
    if (col >= cards_.size()) { v.bind(nullptr, nullptr, false); continue; }
    v.set_pos(kRowMargin + static_cast<int>(i) * step_, header_h_);
    v.bind(&cards_[col], images.get(cards_[col].poster), images.failed(cards_[col].poster));
    v.set_selected(static_cast<int>(col) == selected_col);
  }
}

void CardRowView::hide() { lv_obj_add_flag(box_, LV_OBJ_FLAG_HIDDEN); }

void CardRowView::collect_wants(size_t left, size_t count, int priority, std::vector<ImageLoader::Want>& out) const {
  if (kind_ == Kind::Tiles) return;
  for (size_t c = left; c < left + count && c < cards_.size(); ++c) out.push_back({cards_[c].poster, priority});
}

// --------------------------------------------------------------- PosterGridView

PosterGridView::PosterGridView(lv_obj_t* parent, const Fonts& fonts, int x, int y, int height)
    : height_(height) {
  view_ = make_plain_obj(parent, x, y, kScreenW, height);
  for (int k = 0; k < 2; ++k) {
    rows_.push_back(std::make_unique<CardRowView>(view_, fonts, CardRowView::Kind::Posters, kGridCols, kGridStep));
    rows_.back()->hide();
  }
  message_ = make_label(view_, fonts.normal, color::kDim, 10, 10, kScreenW - 20, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_align(message_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
}

void PosterGridView::set_cards(std::vector<CardModel> cards) {
  cards_ = std::move(cards);
  if (!cards_.empty()) selected_ = std::min(selected_, cards_.size() - 1);
  else selected_ = 0;
  clear_message();
}

void PosterGridView::set_selected(size_t index, bool active) {
  selected_ = cards_.empty() ? 0 : std::min(index, cards_.size() - 1);
  active_ = active;
}

void PosterGridView::set_message(const std::string& text, uint32_t color) {
  message_on_ = true;
  lv_label_set_text(message_, text.c_str());
  lv_obj_set_style_text_color(message_, lv_color_hex(color), 0);
  lv_obj_update_layout(message_);
  lv_obj_set_y(message_, std::max(4, (height_ - lv_obj_get_height(message_)) / 2));
  set_visible(message_, true);
  for (auto& r : rows_) r->hide();
}

void PosterGridView::clear_message() {
  message_on_ = false;
  set_visible(message_, false);
}

void PosterGridView::refresh(ImageLoader& images) {
  if (message_on_) return;
  const size_t sel_row = selected_row();
  for (size_t k = 0; k < rows_.size(); ++k) {
    const size_t start = (sel_row + k) * kGridCols;
    if (start >= cards_.size()) { rows_[k]->hide(); continue; }
    const size_t end = std::min(cards_.size(), start + kGridCols);
    rows_[k]->set_cards(std::vector<CardModel>(cards_.begin() + static_cast<long>(start), cards_.begin() + static_cast<long>(end)));
    rows_[k]->set_y(static_cast<int>(k) * kGridRowH);
    rows_[k]->show(images, 0, (k == 0 && active_) ? static_cast<int>(selected_ % kGridCols) : -1);
  }
}

std::vector<ImageLoader::Want> PosterGridView::wants() const {
  std::vector<ImageLoader::Want> out;
  const size_t sel_row = selected_row();
  auto add_row = [&](size_t row, int priority) {
    for (size_t c = row * kGridCols; c < (row + 1) * kGridCols && c < cards_.size(); ++c) {
      out.push_back({cards_[c].poster, priority});
    }
  };
  add_row(sel_row, 0);
  add_row(sel_row + 1, 1);
  if (sel_row > 0) add_row(sel_row - 1, 2);
  return out;
}

}  // namespace zerofin::app
