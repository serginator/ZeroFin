#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lvgl.h"
#include "ui/image_loader.h"
#include "ui/theme.h"
#include "ui_core/cards.h"

namespace zerofin::app {

constexpr int kCardW = ImageLoader::kPosterW;
constexpr int kCardH = ImageLoader::kPosterH;
constexpr int kCardStep = 70;      // x distance between posters in a row
constexpr int kTileStep = 60;      // x distance between tiles
constexpr int kRowMargin = 8;
constexpr int kHeaderH = 16;
constexpr int kCaptionH = 14;
constexpr int kTileBlockH = 64;
constexpr int kGridRowH = kCardH + kCaptionH + 2;
constexpr int kGridCols = 4;
constexpr int kGridStep = 78;

struct CardModel {
  std::string key;  // the item id
  ui::CardText text;
  jf::ImageRef poster;
};

struct TileModel {
  std::string key;
  std::string label;
  ui::TileKind kind = ui::TileKind::Other;
  uint32_t tint = color::kAccent;
};

// One poster: a picture (or a placeholder with the first letter), a progress bar, an episode badge, a
// watched mark, and the title under it while it is selected.
class PosterCardView {
 public:
  PosterCardView(lv_obj_t* row, const Fonts& fonts);
  void set_pos(int x, int y);
  // A null model hides the card. `poster` is null until the image arrives.
  void bind(const CardModel* model, ImageLoader::PosterPtr poster, bool failed);
  void set_selected(bool selected);

 private:
  lv_obj_t* frame_ = nullptr;
  lv_obj_t* img_ = nullptr;
  lv_obj_t* letter_ = nullptr;
  lv_obj_t* badge_ = nullptr;
  lv_obj_t* check_ = nullptr;
  lv_obj_t* bar_ = nullptr;
  lv_obj_t* bar_fill_ = nullptr;
  lv_obj_t* caption_ = nullptr;
  ImageLoader::PosterPtr poster_;  // keeps the picture data alive while LVGL shows it
  bool bound_ = false;
};

// A tile: three stacked cards (a "collection"), a symbol on the front card, and the name under it.
class TileView {
 public:
  TileView(lv_obj_t* row, const Fonts& fonts);
  void set_pos(int x, int y);
  void bind(const TileModel* model);  // null hides the tile
  void set_selected(bool selected);

 private:
  lv_obj_t* back_ = nullptr;
  lv_obj_t* mid_ = nullptr;
  lv_obj_t* front_ = nullptr;
  lv_obj_t* glyph_ = nullptr;
  lv_obj_t* lens_ = nullptr;
  lv_obj_t* handle_ = nullptr;
  lv_obj_t* name_ = nullptr;
};

// A horizontal row of tiles or posters with an optional header. Only `pool` cards exist as LVGL objects
// (the visible ones and one more that shows at the edge). They are bound again when the window moves.
class CardRowView {
 public:
  enum class Kind { Tiles, Posters };
  CardRowView(lv_obj_t* parent, const Fonts& fonts, Kind kind, size_t pool, int step);
  // Deletes the LVGL objects of the row and of all its cards. A row that is dropped must leave the screen.
  ~CardRowView();
  CardRowView(const CardRowView&) = delete;
  CardRowView& operator=(const CardRowView&) = delete;

  void set_header(const std::string& text);
  void set_y(int y);
  int height() const;
  void set_cards(std::vector<CardModel> cards);
  void set_tiles(std::vector<TileModel> tiles);
  size_t size() const { return kind_ == Kind::Tiles ? tiles_.size() : cards_.size(); }

  // Shows the cards from column `left`. `selected_col` < 0 draws no selection.
  void show(ImageLoader& images, size_t left, int selected_col);
  void hide();
  // The posters of the columns [left, left + count), with `priority`.
  void collect_wants(size_t left, size_t count, int priority, std::vector<ImageLoader::Want>& out) const;

 private:
  Kind kind_;
  int step_;
  int header_h_ = 0;
  lv_obj_t* box_ = nullptr;
  lv_obj_t* header_ = nullptr;
  std::vector<CardModel> cards_;
  std::vector<TileModel> tiles_;
  std::vector<std::unique_ptr<PosterCardView>> card_views_;
  std::vector<std::unique_ptr<TileView>> tile_views_;
};

// Posters in rows of four, for the lists of movies, series and seasons and for Search. The selected row
// is at the top. The next row shows below it.
class PosterGridView {
 public:
  PosterGridView(lv_obj_t* parent, const Fonts& fonts, int x, int y, int height);

  void set_cards(std::vector<CardModel> cards);
  size_t size() const { return cards_.size(); }
  // `index` is the position in the whole list. `active` false draws no selection.
  void set_selected(size_t index, bool active = true);
  // Rebinds the visible posters. Call it after a change and when a poster arrives.
  void refresh(ImageLoader& images);
  // A centered message (Loading..., No results). It hides the posters.
  void set_message(const std::string& text, uint32_t color = color::kDim);
  void clear_message();
  std::vector<ImageLoader::Want> wants() const;

 private:
  size_t selected_row() const { return selected_ / kGridCols; }

  int height_;
  lv_obj_t* view_ = nullptr;
  lv_obj_t* message_ = nullptr;
  std::vector<std::unique_ptr<CardRowView>> rows_;  // the selected row and the one below
  std::vector<CardModel> cards_;
  size_t selected_ = 0;
  bool active_ = true;
  bool message_on_ = false;
};

}  // namespace zerofin::app
