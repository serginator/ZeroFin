#include <algorithm>
#include <optional>

#include "screens/screens.h"
#include "ui/widgets.h"
#include "ui_core/format.h"

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

// The first version shows movies and TV. Other library types are left out.
bool library_supported(const jf::Item& view) {
  const std::string& t = view.collection_type;
  return t.empty() || t == "movies" || t == "tvshows" || t == "boxsets" || t == "homevideos" || t == "mixed";
}

// ----------------------------------------------------------------------- home

class HomeScreen : public Screen {
 public:
  explicit HomeScreen(AppContext& ctx) : Screen(ctx, ctx.config.server_name.empty() ? "ZeroFin" : ctx.config.server_name) {}

  void build() override {
    list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH, kScreenW, 6);
    set_status(ctx_.config.user_name);
    list_->set_message("Loading...");
  }

  void on_show() override { load(); }

  bool on_key(const ui::KeyEvent& e) override {
    if (list_->handle_key(e)) return true;
    if (e.key == ui::Key::Enter && !e.repeat) {
      const int s = list_->selected();
      if (s >= 0 && static_cast<size_t>(s) < actions_.size() && actions_[static_cast<size_t>(s)]) {
        actions_[static_cast<size_t>(s)]();
      } else if (failed_) {
        load();
      }
      return true;
    }
    if (e.key == ui::Key::Tab) {
      ctx_.nav->push(make_search_screen(ctx_));
      return true;
    }
    if (e.key == ui::Key::Char && (e.ch == 'r' || e.ch == 'R')) {
      load();
      return true;
    }
    return false;
  }

 private:
  void load() {
    if (pending_ > 0) return;
    pending_ = 3;
    failed_ = false;
    first_error_.reset();
    auto client = ctx_.client;
    async([client] { return client->resume_items(10); }, [this](jf::Result<jf::ItemList> r) {
      if (r) staged_resume_ = std::move(r.value().items); else note_error(r.error());
      done_one();
    });
    async([client] { return client->next_up(10); }, [this](jf::Result<jf::ItemList> r) {
      if (r) staged_next_ = std::move(r.value().items); else note_error(r.error());
      done_one();
    });
    async([client] { return client->views(); }, [this](jf::Result<jf::ItemList> r) {
      if (r) staged_views_ = std::move(r.value().items); else note_error(r.error());
      done_one();
    });
  }

  void note_error(const jf::Error& e) {
    if (!first_error_) first_error_ = e;
  }

  void done_one() {
    if (--pending_ > 0) return;
    if (first_error_ && handle_auth_error(ctx_, *first_error_)) return;
    // The row actions index into these lists, so they change only together with the rows.
    auto take = [](std::optional<std::vector<jf::Item>>& staged, std::vector<jf::Item>& into) {
      if (!staged) return;
      into = std::move(*staged);
      staged.reset();
    };
    take(staged_resume_, resume_);
    take(staged_next_, next_);
    take(staged_views_, views_);
    failed_ = first_error_.has_value() && views_.empty();
    rebuild();
  }

  void rebuild() {
    if (failed_) {
      list_->set_message(error_text(*first_error_) + "\n\nEnter: try again", color::kError);
      actions_.clear();
      return;
    }
    std::vector<ListRow> rows;
    actions_.clear();
    auto header = [&](const std::string& text) {
      ListRow r;
      r.header = true;
      r.primary = text;
      rows.push_back(std::move(r));
      actions_.emplace_back();
    };
    auto add_items = [&](const std::vector<jf::Item>& items) {
      for (size_t i = 0; i < items.size(); ++i) {
        rows.push_back(to_row(ui::describe_item(items[i], ui::RowContext::Continue)));
        actions_.push_back([this, list = &items, i] { open_item(ctx_, (*list)[i]); });
      }
    };

    if (!resume_.empty()) {
      header("Continue watching");
      add_items(resume_);
    }
    if (!next_.empty()) {
      header("Next up");
      add_items(next_);
    }
    header("Libraries");
    for (size_t i = 0; i < views_.size(); ++i) {
      if (!library_supported(views_[i])) continue;
      ListRow r;
      r.primary = views_[i].name;
      rows.push_back(std::move(r));
      actions_.push_back([this, i] { open_item(ctx_, views_[i]); });
    }
    header("More");
    {
      ListRow r;
      r.primary = "Search";
      r.detail = "Tab";
      rows.push_back(std::move(r));
      actions_.push_back([this] { ctx_.nav->push(make_search_screen(ctx_)); });
    }
    {
      ListRow r;
      r.primary = "Settings";
      rows.push_back(std::move(r));
      actions_.push_back([this] { ctx_.nav->push(make_settings_screen(ctx_)); });
    }
    list_->set_rows(std::move(rows));
  }

  std::unique_ptr<ListView> list_;
  std::vector<jf::Item> resume_;
  std::vector<jf::Item> next_;
  std::vector<jf::Item> views_;
  // Fresh results of a load. They replace the lists above when all three requests are done.
  std::optional<std::vector<jf::Item>> staged_resume_;
  std::optional<std::vector<jf::Item>> staged_next_;
  std::optional<std::vector<jf::Item>> staged_views_;
  std::vector<std::function<void()>> actions_;
  std::optional<jf::Error> first_error_;
  int pending_ = 0;
  bool failed_ = false;
};

// ------------------------------------------------------------------ item list

class ItemListScreen : public Screen {
 public:
  ItemListScreen(AppContext& ctx, ItemListSpec spec) : Screen(ctx, spec.title), spec_(std::move(spec)) {}

  void build() override {
    list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH, kScreenW, 6);
    list_->set_message("Loading...");
  }

  void on_show() override {
    if (!started_) {
      started_ = true;
      load_more();
    }
  }

  bool on_key(const ui::KeyEvent& e) override {
    if (list_->handle_key(e)) {
      update_status();
      if (list_->near_end(6)) load_more();
      return true;
    }
    if (e.key == ui::Key::Enter && !e.repeat) {
      if (items_.empty()) {
        if (failed_) load_more();
        return true;
      }
      const int s = list_->selected();
      if (s >= 0 && spec_.on_select) spec_.on_select(items_[static_cast<size_t>(s)]);
      return true;
    }
    return false;
  }

 private:
  bool all_loaded() const { return !spec_.paged || static_cast<int>(items_.size()) >= total_; }

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
          list_->set_message(error_text(r.error()) + "\n\nEnter: try again", color::kError);
        } else {
          ctx_.toast->show(error_text(r.error()), Toast::Kind::Error);
        }
        return;
      }
      auto page = std::move(r.value());
      const bool empty_page = page.items.empty();
      for (auto& it : page.items) items_.push_back(std::move(it));
      total_ = spec_.paged ? (empty_page ? static_cast<int>(items_.size()) : page.total) : static_cast<int>(items_.size());
      rebuild();
      if (list_->near_end(6) && !all_loaded()) load_more();
    });
  }

  void rebuild() {
    if (items_.empty()) {
      list_->set_message(spec_.empty_text);
      set_status("");
      return;
    }
    std::vector<ListRow> rows;
    rows.reserve(items_.size());
    for (const auto& it : items_) rows.push_back(to_row(ui::describe_item(it, spec_.context)));
    list_->set_rows(std::move(rows));
    update_status();
  }

  void update_status() {
    if (items_.empty()) return;
    set_status(std::to_string(list_->selected() + 1) + " / " + std::to_string(std::max<int>(total_, static_cast<int>(items_.size()))));
  }

  ItemListSpec spec_;
  std::unique_ptr<ListView> list_;
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
    list_ = std::make_unique<ListView>(root_, ctx_.fonts, 0, kTitleH + 34, kScreenW, 4);
    list_->set_message("Type a title");
    list_->set_active(false);
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

    if ((e.key == ui::Key::Up && list_->selected() == 0) || e.key == ui::Key::Tab) {
      focus_list(false);
      return true;
    }
    if (list_->handle_key(e)) return true;
    if (e.key == ui::Key::Enter && !e.repeat) {
      const int s = list_->selected();
      if (s >= 0) open_item(ctx_, results_[static_cast<size_t>(s)]);
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
    list_->set_active(on);
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
      list_->set_message("Type a title");
      set_status("");
      return;
    }
    set_status("Searching...");
    const int id = request_;
    auto client = ctx_.client;
    async([client, term] { return client->search(term, 30); }, [this, id](jf::Result<jf::ItemList> r) {
      if (id != request_) return;  // an newer search replaced this one
      set_status("");
      if (!r) {
        if (handle_auth_error(ctx_, r.error())) return;
        results_.clear();
        list_->set_message(error_text(r.error()), color::kError);
        return;
      }
      results_ = std::move(r.value().items);
      if (results_.empty()) {
        list_->set_message("No results");
        return;
      }
      std::vector<ListRow> rows;
      for (const auto& it : results_) {
        ListRow row = to_row(ui::describe_item(it, ui::RowContext::Browse));
        if (it.type == jf::ItemType::Series) row.detail = row.detail.empty() ? "Series" : "Series " + row.detail;
        rows.push_back(std::move(row));
      }
      list_->set_rows(std::move(rows));
      list_->set_active(in_list_);
      set_status(std::to_string(results_.size()) + (results_.size() == 1 ? " result" : " results"));
    });
  }

  std::unique_ptr<TextFieldView> field_;
  std::unique_ptr<ListView> list_;
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
