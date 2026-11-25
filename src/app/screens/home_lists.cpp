#include <algorithm>
#include <iterator>
#include <map>
#include <optional>

#include "screens/screens.h"
#include "ui/image_loader.h"
#include "ui/poster_widgets.h"
#include "ui/widgets.h"
#include "ui_core/cards.h"
#include "ui_core/format.h"
#include "ui_core/grid_cursor.h"

namespace zerofin::app {
namespace {

constexpr int kPageSize = 40;

ListRow to_row(const ui::RowText& t) {
  ListRow r;
  r.primary = t.primary;
  r.detail = t.detail;
  r.progress = t.progress;
  r.watched = t.watched;
  return r;
}

// ----------------------------------------------------------------------- home

constexpr size_t kTilesVisible = 5;
constexpr size_t kPostersVisible = 4;
constexpr uint32_t kFullReloadMs = 5 * 60 * 1000;

// Tiles for the libraries, then rows of posters: Keep watching, Next up, Recent in <library>.
class HomeScreen : public Screen {
 public:
  explicit HomeScreen(AppContext& ctx) : Screen(ctx, ctx.config.server_name.empty() ? "ZeroFin" : ctx.config.server_name) {}

  void build() override {
    content_ = make_plain_obj(root_, 0, kTitleH, kScreenW, kContentH);
    inner_ = make_plain_obj(content_, 0, 0, kScreenW, kContentH);
    message_ = make_label(content_, ctx_.fonts.normal, color::kDim, 10, 10, kScreenW - 20, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(message_, LV_TEXT_ALIGN_CENTER, 0);
    set_status(ctx_.config.user_name);
    set_message("Loading...", color::kDim);
  }

  void on_show() override {
    if (!rows_.empty()) layout();  // back from another screen: draw at once, the posters are in memory
    load(false);
  }

  bool on_key(const ui::KeyEvent& e) override {
    switch (e.key) {
      case ui::Key::Up:
      case ui::Key::Down:
      case ui::Key::Left:
      case ui::Key::Right:
      case ui::Key::Home:
      case ui::Key::End:
      case ui::Key::PageUp:
      case ui::Key::PageDown:
        if (cursor_.move(e.key)) layout();
        return true;
      case ui::Key::Enter:
        if (e.repeat) return true;
        if (failed_) load(true); else activate();
        return true;
      case ui::Key::Tab:
        ctx_.nav->push(make_search_screen(ctx_));
        return true;
      case ui::Key::Char:
        if (e.ch == 'r' || e.ch == 'R') {
          ctx_.images->forget_failures();
          load(true);
          return true;
        }
        return false;
      default:
        return false;
    }
  }

 private:
  struct Row {
    std::string key;
    bool tiles = false;
    std::vector<jf::Item> items;                        // poster rows
    std::vector<std::function<void()>> tile_actions;    // tile rows
    std::vector<std::string> item_keys;
    std::unique_ptr<CardRowView> view;
  };

  void set_message(const std::string& text, uint32_t color) {
    lv_label_set_text(message_, text.c_str());
    lv_obj_set_style_text_color(message_, lv_color_hex(color), 0);
    lv_obj_update_layout(message_);
    lv_obj_set_y(message_, std::max(4, (kContentH - lv_obj_get_height(message_)) / 2));
    lv_obj_remove_flag(message_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(inner_, LV_OBJ_FLAG_HIDDEN);
  }

  void load(bool full) {
    if (pending_ > 0) return;
    full = full || !views_ready_ || lv_tick_elaps(last_full_) > kFullReloadMs;
    failed_ = false;
    first_error_.reset();
    auto client = ctx_.client;
    fetch([client] { return client->resume_items(12); }, [this](std::vector<jf::Item> v) { resume_ = std::move(v); });
    fetch([client] { return client->next_up(12); }, [this](std::vector<jf::Item> v) { next_ = std::move(v); });
    if (!full) return;
    fetch([client] { return client->views(); }, [this](std::vector<jf::Item> v) {
      views_ = std::move(v);
      views_ready_ = true;
      last_full_ = lv_tick_get();
      for (const auto& view : views_) {
        if (ui::has_latest_row(view)) fetch_latest(view.id);
      }
    });
  }

  void fetch_latest(const std::string& library_id) {
    auto client = ctx_.client;
    fetch([client, library_id] { return client->latest(library_id, 16); },
          [this, library_id](std::vector<jf::Item> v) { latest_[library_id] = std::move(v); });
  }

  // Starts one request. `store` runs on the UI thread with the items. It may start more requests.
  void fetch(std::function<jf::Result<jf::ItemList>()> work, std::function<void(std::vector<jf::Item>)> store) {
    ++pending_;
    async(std::move(work), [this, store = std::move(store)](jf::Result<jf::ItemList> r) {
      if (r) store(std::move(r.value().items)); else if (!first_error_) first_error_ = r.error();
      done_one();
    });
  }

  void done_one() {
    --pending_;
    if (pending_ == 0) {
      if (first_error_ && handle_auth_error(ctx_, *first_error_)) return;
      failed_ = first_error_.has_value() && !views_ready_;
    }
    if (views_ready_ || failed_) rebuild();
  }

  void add_poster_row(const std::string& key, const std::string& header, const std::vector<jf::Item>& items) {
    if (items.empty()) return;
    Row r;
    r.key = key;
    r.items = items;
    std::vector<CardModel> cards;
    for (const auto& it : items) {
      cards.push_back(CardModel{it.id, ui::describe_card(it), jf::poster_ref(it)});
      r.item_keys.push_back(it.id);
    }
    r.view = std::make_unique<CardRowView>(inner_, ctx_.fonts, CardRowView::Kind::Posters, kPostersVisible + 1, kCardStep);
    r.view->set_header(header);
    r.view->set_cards(std::move(cards));
    rows_.push_back(std::move(r));
  }

  void add_tile_row() {
    Row r;
    r.key = "tiles";
    r.tiles = true;
    std::vector<TileModel> tiles;
    size_t n = 0;
    for (const auto& v : views_) {
      if (!ui::library_supported(v)) continue;
      tiles.push_back(TileModel{"lib:" + v.id, v.name, ui::tile_kind(v), color::kTilePalette[n++ % std::size(color::kTilePalette)]});
      r.tile_actions.push_back([this, v] { open_item(ctx_, v); });
    }
    tiles.push_back(TileModel{"search", "Search", ui::TileKind::Search, color::kTileNeutral});
    r.tile_actions.push_back([this] { ctx_.nav->push(make_search_screen(ctx_)); });
    tiles.push_back(TileModel{"settings", "Settings", ui::TileKind::Settings, color::kTileNeutral});
    r.tile_actions.push_back([this] { ctx_.nav->push(make_settings_screen(ctx_)); });
    for (const auto& t : tiles) r.item_keys.push_back(t.key);
    r.view = std::make_unique<CardRowView>(inner_, ctx_.fonts, CardRowView::Kind::Tiles, kTilesVisible + 1, kTileStep);
    r.view->set_tiles(std::move(tiles));
    rows_.push_back(std::move(r));
  }

  void rebuild() {
    if (failed_) {
      rows_.clear();
      set_message(error_text(*first_error_) + "\n\nEnter: try again", color::kError);
      return;
    }
    // Remember where the cursor is, to put it back on the same card after the rows are built again.
    std::string old_row, old_item;
    const size_t old_col = cursor_.col();
    if (!rows_.empty() && !cursor_.empty() && cursor_.row() < rows_.size()) {
      old_row = rows_[cursor_.row()].key;
      if (old_col < rows_[cursor_.row()].item_keys.size()) old_item = rows_[cursor_.row()].item_keys[old_col];
    }
    rows_.clear();
    add_tile_row();
    add_poster_row("resume", "Keep watching", resume_);
    add_poster_row("next", "Next up", next_);
    for (const auto& v : views_) {
      if (!ui::has_latest_row(v)) continue;
      if (auto it = latest_.find(v.id); it != latest_.end()) add_poster_row("latest:" + v.id, "Recent in " + v.name, it->second);
    }

    std::vector<ui::GridRow> grid;
    std::vector<ui::RowKeys> keys;
    for (const auto& r : rows_) {
      grid.push_back({r.view->size(), r.tiles ? kTilesVisible : kPostersVisible});
      keys.push_back({r.key, r.item_keys});
    }
    cursor_.set_rows(std::move(grid));
    if (!old_row.empty()) {
      if (auto pos = ui::find_position(keys, old_row, old_item, old_col)) cursor_.select(pos->first, pos->second);
    }
    lv_obj_add_flag(message_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(inner_, LV_OBJ_FLAG_HIDDEN);
    layout();
  }

  // Puts the rows under each other, scrolls so the selected row is at the top, binds the cards that show,
  // and tells the image loader which posters to fetch first.
  void layout() {
    if (rows_.empty() || cursor_.empty()) return;
    int y = 0;
    std::vector<int> tops;
    for (auto& r : rows_) {
      tops.push_back(y);
      r.view->set_y(y);
      y += r.view->height();
    }
    lv_obj_set_height(inner_, y);
    const size_t sel = std::min(cursor_.row(), rows_.size() - 1);
    lv_obj_set_y(inner_, -tops[sel]);

    std::vector<ImageLoader::Want> wants;
    for (size_t i = 0; i < rows_.size(); ++i) {
      Row& r = rows_[i];
      if (i + 1 < sel || i > sel + 1) {
        r.view->hide();
        continue;
      }
      const size_t left = cursor_.left(i);
      const size_t visible = r.tiles ? kTilesVisible : kPostersVisible;
      r.view->show(*ctx_.images, left, i == sel ? static_cast<int>(cursor_.col()) : -1);
      r.view->collect_wants(left, visible + 1, i == sel ? 0 : (i == sel + 1 ? 1 : 3), wants);
      if (i == sel) r.view->collect_wants(left + visible + 1, 4, 2, wants);
    }
    ctx_.images->want(alive(), std::move(wants), ctx_.client, [this] { layout(); });
  }

  void activate() {
    if (rows_.empty() || cursor_.empty()) return;
    Row& r = rows_[std::min(cursor_.row(), rows_.size() - 1)];
    const size_t col = cursor_.col();
    if (r.tiles) {
      if (col < r.tile_actions.size()) r.tile_actions[col]();
    } else if (col < r.items.size()) {
      open_item(ctx_, r.items[col]);
    }
  }

  lv_obj_t* content_ = nullptr;
  lv_obj_t* inner_ = nullptr;
  lv_obj_t* message_ = nullptr;
  std::vector<Row> rows_;
  ui::GridCursor cursor_;
  std::vector<jf::Item> resume_;
  std::vector<jf::Item> next_;
  std::vector<jf::Item> views_;
  std::map<std::string, std::vector<jf::Item>> latest_;
  std::optional<jf::Error> first_error_;
  int pending_ = 0;
  bool failed_ = false;
  bool views_ready_ = false;
  uint32_t last_full_ = 0;
};

// ------------------------------------------------------------------ poster grids

// Rows of kGridCols cards. The last row can be shorter.
std::vector<ui::GridRow> grid_rows(size_t count) {
  std::vector<ui::GridRow> rows;
  for (size_t left = count; left > 0;) {
    const size_t n = std::min<size_t>(left, kGridCols);
    rows.push_back({n, kGridCols});
    left -= n;
  }
  return rows;
}

std::vector<CardModel> to_cards(const std::vector<jf::Item>& items) {
  std::vector<CardModel> cards;
  cards.reserve(items.size());
  for (const auto& it : items) cards.push_back(CardModel{it.id, ui::describe_card(it), jf::poster_ref(it)});
  return cards;
}

bool is_grid_key(ui::Key k) {
  return k == ui::Key::Up || k == ui::Key::Down || k == ui::Key::Left || k == ui::Key::Right || k == ui::Key::Home ||
         k == ui::Key::End || k == ui::Key::PageUp || k == ui::Key::PageDown;
}

// The position in the whole list of the card the cursor is on.
size_t flat_index(const ui::GridCursor& c) { return c.row() * kGridCols + c.col(); }

// ------------------------------------------------------------------ item list

class ItemListScreen : public Screen {
 public:
  ItemListScreen(AppContext& ctx, ItemListSpec spec) : Screen(ctx, spec.title), spec_(std::move(spec)) {}

  void build() override {
    if (spec_.posters) {
      grid_ = std::make_unique<PosterGridView>(root_, ctx_.fonts, 0, kTitleH, kContentH);
    } else {
      list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH, kScreenW, 6);
    }
    set_msg("Loading...", color::kDim);
  }

  void on_show() override {
    if (!started_) {
      started_ = true;
      load_more();
    } else if (grid_) {
      refresh_grid();  // back from another screen
    }
  }

  bool on_key(const ui::KeyEvent& e) override {
    const bool nav = grid_ ? handle_grid_key(e) : list_->handle_key(e);
    if (nav) {
      update_status();
      // After a failed page, wait for R: a key press must not start a new request and a new toast each time.
      if (near_end() && !failed_) load_more();
      return true;
    }
    if (failed_ && !items_.empty() && e.key == ui::Key::Char && (e.ch == 'r' || e.ch == 'R')) {
      load_more();
      return true;
    }
    if (e.key == ui::Key::Enter && !e.repeat) {
      if (items_.empty()) {
        if (failed_) load_more();
        return true;
      }
      const int s = selected_index();
      if (s >= 0 && spec_.on_select) spec_.on_select(items_[static_cast<size_t>(s)]);
      return true;
    }
    return false;
  }

 private:
  bool all_loaded() const { return !spec_.paged || static_cast<int>(items_.size()) >= total_; }

  int selected_index() const {
    if (items_.empty()) return -1;
    return grid_ ? static_cast<int>(flat_index(cursor_)) : list_->selected();
  }

  bool near_end() const {
    if (!grid_) return list_->near_end(6);
    return !items_.empty() && flat_index(cursor_) + 2 * kGridCols >= items_.size();
  }

  bool handle_grid_key(const ui::KeyEvent& e) {
    if (!is_grid_key(e.key)) return false;
    if (cursor_.move(e.key)) refresh_grid();
    return true;
  }

  void set_msg(const std::string& text, uint32_t color) {
    if (grid_) grid_->set_message(text, color); else list_->set_message(text, color);
  }

  void refresh_grid() {
    grid_->set_selected(flat_index(cursor_));
    grid_->refresh(*ctx_.images);
    ctx_.images->want(alive(), grid_->wants(), ctx_.client, [this] { grid_->refresh(*ctx_.images); });
  }

  void load_more() {
    if (loading_ || (!items_.empty() && all_loaded())) return;
    loading_ = true;
    failed_ = false;
    const int start = static_cast<int>(items_.size());
    auto loader = spec_.loader;
    async([loader, start] { return loader(start, kPageSize); }, [this](jf::Result<jf::ItemList> r) {
      loading_ = false;
      if (!r) {
        if (handle_auth_error(ctx_, r.error())) return;
        failed_ = true;
        if (items_.empty()) {
          set_msg(error_text(r.error()) + "\n\nEnter: try again", color::kError);
        } else {
          ctx_.toast->show(error_text(r.error()) + ". R: retry", Toast::Kind::Error);
        }
        return;
      }
      auto page = std::move(r.value());
      const bool empty_page = page.items.empty();
      for (auto& it : page.items) items_.push_back(std::move(it));
      total_ = spec_.paged ? (empty_page ? static_cast<int>(items_.size()) : page.total) : static_cast<int>(items_.size());
      rebuild();
      if (near_end() && !all_loaded()) load_more();
    });
  }

  void rebuild() {
    if (items_.empty()) {
      set_msg(spec_.empty_text, color::kDim);
      set_status("");
      return;
    }
    if (grid_) {
      grid_->set_cards(to_cards(items_));
      cursor_.set_rows(grid_rows(items_.size()));
      refresh_grid();
    } else {
      std::vector<ListRow> rows;
      rows.reserve(items_.size());
      for (const auto& it : items_) rows.push_back(to_row(ui::describe_item(it, spec_.context)));
      list_->set_rows(std::move(rows));
    }
    update_status();
  }

  void update_status() {
    if (items_.empty()) return;
    set_status(std::to_string(selected_index() + 1) + " / " + std::to_string(std::max<int>(total_, static_cast<int>(items_.size()))));
  }

  ItemListSpec spec_;
  std::unique_ptr<ListView> list_;
  std::unique_ptr<PosterGridView> grid_;
  ui::GridCursor cursor_;
  std::vector<jf::Item> items_;
  int total_ = 0;
  bool started_ = false;
  bool loading_ = false;
  bool failed_ = false;
};

// ---------------------------------------------------------------------- search

class SearchScreen : public Screen {
 public:
  bool takes_text() const override { return true; }
  explicit SearchScreen(AppContext& ctx) : Screen(ctx, "Search") {}

  void build() override {
    field_ = std::make_unique<TextFieldView>(root_, ctx_.fonts, 8, kTitleH + 4, 304, 26);
    field_->set_placeholder("Type to search");
    grid_ = std::make_unique<PosterGridView>(root_, ctx_.fonts, 0, kTitleH + 34, kContentH - 34);
    grid_->set_message("Type a title");
  }

  void on_show() override {
    refresh_grid();
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (!in_list_) {
      if (e.key == ui::Key::Down || e.key == ui::Key::Tab || (e.key == ui::Key::Enter && !results_.empty())) {
        if (!results_.empty()) focus_list(true);
        return true;
      }
      if (field_->handle_key(e)) {
        schedule();
        return true;
      }
      if (e.key == ui::Key::Enter) {
        run();
        return true;
      }
      return false;
    }

    if ((e.key == ui::Key::Up && cursor_.row() == 0) || e.key == ui::Key::Tab) {
      focus_list(false);
      return true;
    }
    if (is_grid_key(e.key)) {
      if (cursor_.move(e.key)) refresh_grid();
      return true;
    }
    if (e.key == ui::Key::Enter && !e.repeat) {
      const size_t s = flat_index(cursor_);
      if (s < results_.size()) open_item(ctx_, results_[s]);
      return true;
    }
    if (e.key == ui::Key::Char || e.key == ui::Key::Backspace) {
      focus_list(false);
      if (field_->handle_key(e)) schedule();
      return true;
    }
    return false;
  }

 private:
  void focus_list(bool on) {
    in_list_ = on;
    field_->set_focused(!on);
    refresh_grid();
  }

  void refresh_grid() {
    if (results_.empty()) return;
    grid_->set_selected(flat_index(cursor_), in_list_);
    grid_->refresh(*ctx_.images);
    ctx_.images->want(alive(), grid_->wants(), ctx_.client, [this] { grid_->refresh(*ctx_.images); });
  }

  void schedule() {
    if (debounce_) remove_timer(debounce_);
    debounce_ = add_timer(450, [this] {
      remove_timer(debounce_);
      debounce_ = nullptr;
      run();
    });
  }

  void run() {
    const std::string term = field_->text();
    ++request_;
    if (term.empty()) {
      results_.clear();
      grid_->set_message("Type a title");
      set_status("");
      return;
    }
    set_status("Searching...");
    const int id = request_;
    auto client = ctx_.client;
    async([client, term] { return client->search(term, 30); }, [this, id](jf::Result<jf::ItemList> r) {
      if (id != request_) return;  // a newer search replaced this one
      set_status("");
      if (!r) {
        if (handle_auth_error(ctx_, r.error())) return;
        results_.clear();
        grid_->set_message(error_text(r.error()), color::kError);
        return;
      }
      results_ = std::move(r.value().items);
      if (results_.empty()) {
        grid_->set_message("No results");
        return;
      }
      std::vector<CardModel> cards = to_cards(results_);
      for (size_t i = 0; i < results_.size(); ++i) {
        if (results_[i].type == jf::ItemType::Series) cards[i].text.badge = "SERIES";
      }
      grid_->set_cards(std::move(cards));
      cursor_.set_rows({});  // a new search starts at the first poster
      cursor_.set_rows(grid_rows(results_.size()));
      refresh_grid();
      set_status(std::to_string(results_.size()) + (results_.size() == 1 ? " result" : " results"));
    });
  }

  std::unique_ptr<TextFieldView> field_;
  std::unique_ptr<PosterGridView> grid_;
  ui::GridCursor cursor_;
  std::vector<jf::Item> results_;
  lv_timer_t* debounce_ = nullptr;
  int request_ = 0;
  bool in_list_ = false;
};

}  // namespace

std::unique_ptr<Screen> make_home_screen(AppContext& ctx) { return std::make_unique<HomeScreen>(ctx); }
std::unique_ptr<Screen> make_item_list_screen(AppContext& ctx, ItemListSpec spec) {
  return std::make_unique<ItemListScreen>(ctx, std::move(spec));
}
std::unique_ptr<Screen> make_search_screen(AppContext& ctx) { return std::make_unique<SearchScreen>(ctx); }

}  // namespace zerofin::app
