#include "nes.hpp"
#include "gameboy.hpp"

#include "sdkconfig.h"

#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/queue.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "i2c.hpp"
#include "badge_input.h"
#include "i2s_audio.h"
#include "i80_lcd.h"
#include "format.hpp"
#include "task_monitor.hpp"

#include "gbc_cart.hpp"
#include "nes_cart.hpp"
#include "heap_utils.hpp"
#include "string_utils.hpp"
#include "fs_init.h"
#include "gui.hpp"
#include "mmap.hpp"
#include "rom_info.hpp"

// from i80_lcd.cpp
#include "display.hpp"
extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

#define GUI

bool operator==(const InputState& lhs, const InputState& rhs) {
  return
    lhs.a == rhs.a &&
    lhs.b == rhs.b &&
    lhs.x == rhs.x &&
    lhs.y == rhs.y &&
    lhs.select == rhs.select &&
    lhs.start == rhs.start &&
    lhs.up == rhs.up &&
    lhs.down == rhs.down &&
    lhs.left == rhs.left &&
    lhs.right == rhs.right &&
    lhs.joystick_select == rhs.joystick_select;
}

std::unique_ptr<Cart> make_cart(const RomInfo& info) {
  switch (info.platform) {
  case Emulator::GAMEBOY:
  case Emulator::GAMEBOY_COLOR:
    return std::make_unique<GbcCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::INFO
      });
    break;
  case Emulator::NES:
    return std::make_unique<NesCart>(Cart::Config{
        .info = info,
        .display = display,
        .verbosity = espp::Logger::Verbosity::WARN
      });
  default:
    return nullptr;
  }
}

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  fmt::print("Before framebuf, free (DMA):        {}\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
  lcd_init();
  // initialize the i2c buses for touchpad, imu, audio codecs, gamepad, haptics, etc.
  i2c_init();
  // init the audio subsystem
  audio_init();
  // init the input subsystem
  init_input();

  // discover roms in flash
  auto roms = read_roms(MOUNT_POINT "/");
  // for debugging/serial console, also discover saves
  read_roms(SAVE_DIR "/");

#ifdef GUI
  fmt::print("initializing gui...\n");
  // initialize the gui
  Gui gui({
      .display = display,
      .log_level = espp::Logger::Verbosity::DEBUG
    });

  for (auto& rom : roms) {
    gui.add_rom(rom.name);
  }
#endif

  while (true) {
    // reset gui ready to play and user_quit
#ifdef GUI
    gui.ready_to_play(false);

    struct InputState prev_state;
    struct InputState curr_state;
    get_input_state(&prev_state);
    get_input_state(&curr_state);
    while (!gui.ready_to_play()) {
      get_input_state(&curr_state);
      if (curr_state != prev_state) {
        prev_state = curr_state;
        if (curr_state.up) {
          gui.previous();
        } else if (curr_state.down) {
          gui.next();
        } else if (curr_state.start) {
          // same as play button was pressed, just exit the loop!
          break;
        } else if (curr_state.a) {
          break;
        }
      }
      std::this_thread::sleep_for(50ms);
    }

    // Now pause the LVGL gui
    display->pause();
    gui.pause();
#endif

    // ensure the display has been paused
    std::this_thread::sleep_for(100ms);

#ifdef GUI
    auto selected_rom_index = gui.get_selected_rom_index();
#else
    // pretend we selected??
    auto selected_rom_index = 0;
#endif
    if (selected_rom_index < roms.size()) {
      fmt::print("Selected rom:\n");
      fmt::print("  index: {}\n", selected_rom_index);
      auto selected_rom_info = roms[selected_rom_index];
      fmt::print("  name:  {}\n", selected_rom_info.name);
      fmt::print("  path:  {}\n", selected_rom_info.rom_path);
      display_clear();

      // Cart handles platform specific code, state management, etc.
      {
        std::unique_ptr<Cart> cart = std::move(make_cart(selected_rom_info));
        fmt::print("Before emulation, minimum free heap: {}\n", heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
        fmt::print("Before emulation, free (default):    {}\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
        fmt::print("Before emulation, free (8-bit):      {}\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        fmt::print("Before emulation, free (DMA):        {}\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
        fmt::print("Before emulation, free (8-bit|DMA):  {}\n", heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA));

        if (cart) {
          fmt::print("Running cart...\n");
          while (cart->run());
        } else {
          fmt::print("Failed to create cart!\n");
        }
      }
    } else {
      fmt::print("Invalid rom selected!\n");
    }

    std::this_thread::sleep_for(100ms);

    fmt::print("Resuming your regularly scheduled programming...\n");

    fmt::print("During emulation, minimum free heap: {}\n", heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));

    // clear screen if required
    display_clear();
#ifdef GUI
    gui.resume();
    display->force_refresh();
    display->resume();
#endif
  }
}
