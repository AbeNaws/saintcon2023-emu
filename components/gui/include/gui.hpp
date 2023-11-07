#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "event_manager.hpp"
#include "display.hpp"
#include "task.hpp"
#include "logger.hpp"

#include "hal_events.hpp"
#include "i2s_audio.h"
#include "video_setting.hpp"

class Gui {
public:
  struct Config {
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  Gui(const Config& config)
    : display_(config.display),
      logger_({.tag="Gui", .level=config.log_level}) {
    init_ui();
    update_shared_state();
    // now start the gui updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({
        .name = "Gui Task",
        .callback = std::bind(&Gui::update, this, _1, _2),
        .stack_size_bytes = 6 * 1024
      });
    task_->start();
  }

  ~Gui() {
    task_->stop();
    deinit_ui();
  }

  void ready_to_play(bool new_state) {
    ready_to_play_ = new_state;
  }

  bool ready_to_play() {
    return ready_to_play_;
  }

  void set_mute(bool muted);

  void toggle_mute() {
    set_mute(!is_muted());
  }

  void set_audio_level(int new_audio_level);

  int get_audio_level();

  void set_video_setting(VideoSetting setting);

  void add_rom(const std::string& name);

  size_t get_selected_rom_index() {
    return focused_rom_;
  }

  void pause() {
    paused_ = true;
  }
  void resume() {
    update_shared_state();
    paused_ = false;
  }

  void next() {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // focus the next rom
    focused_rom_++;
    if (focused_rom_ >= roms_.size()) focused_rom_ = 0;
    auto rom = roms_[focused_rom_];
    focus_rom(rom);
  }

  void previous() {
    // protect since this function is called from another thread context
    std::lock_guard<std::recursive_mutex> lk(mutex_);
    if (roms_.size() == 0) {
      return;
    }
    // focus the previous rom
    focused_rom_--;
    if (focused_rom_ < 0) focused_rom_ = roms_.size() - 1;
    auto rom = roms_[focused_rom_];
    focus_rom(rom);
  }

  void focus_rom(lv_obj_t *new_focus, bool scroll_to_view=true);

protected:
  void init_ui();
  void deinit_ui();

  void load_rom_screen();

  void update_shared_state() {
    set_mute(is_muted());
    set_audio_level(get_audio_volume());
    set_video_setting(::get_video_setting());
  }

  VideoSetting get_video_setting();

  void on_mute_button_pressed(const std::vector<uint8_t>& data) {
    set_mute(is_muted());
  }

  bool update(std::mutex& m, std::condition_variable& cv) {
    if (!paused_) {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
    {
      using namespace std::chrono_literals;
      std::unique_lock<std::mutex> lk(m);
      cv.wait_for(lk, 16ms);
    }
    // don't want to stop the task
    return false;
  }

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto gui = static_cast<Gui*>(user_data);
    if (!gui) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_PRESSED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      gui->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);
  void on_value_changed(lv_event_t *e);

  // LVLG gui objects
  std::vector<lv_obj_t*> roms_;
  std::atomic<int> focused_rom_{-1};

  lv_anim_t rom_label_animation_template_;
  lv_style_t rom_label_style_;

  std::atomic<bool> ready_to_play_{false};
  std::atomic<bool> paused_{false};
  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;
  espp::Logger logger_;
  std::recursive_mutex mutex_;
};
