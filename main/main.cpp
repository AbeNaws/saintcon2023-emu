#include "nes.hpp"
#include "gameboy.hpp"

#include "sdkconfig.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <stdio.h>

#include "i2s_audio.h"
#include "spi_lcd.h"
#include "format.hpp"
#include "st7789.hpp"
#include "task_monitor.hpp"

#include "heap_utils.hpp"
#include "string_utils.hpp"
#include "fs_init.hpp"
#include "gui.hpp"
#include "mmap.hpp"
#include "lv_port_fs.h"
#include "rom_info.hpp"

// from spi_lcd.cpp
extern std::shared_ptr<espp::Display> display;

using namespace std::chrono_literals;

// NOTE: to see the indev configuration for the esp32 s3 box, look at
// bsp/src/boards/esp32_s3_box.c:56. Regarding whether it uses the FT5x06 or the
// TT21100, it uses the tp_prob function which checks to see if the devie
// addresses for those chips exist on the i2c bus (indev_tp.c:37)

extern "C" void app_main(void) {
  fmt::print("Starting esp-box-emu...\n");

  // init nvs and partition
  init_memory();
  // init filesystem
  fs_init();
  // init the display subsystem
  lcd_init();
  // init the audio subsystem
  audio_init();

  fmt::print("initializing gui...\n");
  // initialize the gui
  Gui gui({
      .display = display
    });

  std::atomic<bool> quit{false};

  fmt::print("initializing the lv FS port...\n");
  lv_port_fs_init();

  // load the metadata.csv file, parse it, and add roms from it
  auto roms = parse_metadata("/littlefs/metadata.csv");
  std::string boxart_prefix = "L:";
  for (auto& rom : roms) {
    gui.add_rom(rom.name, boxart_prefix + rom.boxart_path);
    // gui.next();
  }
  gui.next();
  /*
  while (true) {
    // scroll through the rom list forever :)
    gui.next();
    std::this_thread::sleep_for(5s);
  }
  */
  std::this_thread::sleep_for(2s);

  // test playing audio here...
  while (0) {
    // load wav file
    FILE *fp = fopen("/littlefs/wake.wav", "rb");
    if (NULL == fp) {
        fmt::print("Audio file does't exist");
        break;
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *audio_buffer = (uint8_t*)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (NULL == audio_buffer) {
        fmt::print("No mem for audio buffer");
        break;
    }

    fread(audio_buffer, 1, file_size, fp);
    fclose(fp);
    // write that data to i2s
    while (1) {
      fmt::print("playing audio...\n");
      audio_play_frame(audio_buffer, file_size);
      std::this_thread::sleep_for(1s);
    }
  }

  // Now pause the LVGL gui
  display->pause();
  gui.pause();

  fmt::print("{}\n", espp::TaskMonitor::get_latest_info());
  print_heap_state();

  // ensure the display has been paused
  std::this_thread::sleep_for(500ms);

  auto selected_rom_index = gui.get_selected_rom_index();
  auto selected_rom_info = roms[selected_rom_index];

  // copy the rom into the nesgame partition and memory map it
  std::string fs_prefix = "/littlefs/";
  std::string rom_filename = fs_prefix + selected_rom_info.rom_path;
  size_t rom_size_bytes = copy_romdata_to_nesgame_partition(rom_filename);
  while (!rom_size_bytes) {
    fmt::print("Could not copy {} into nesgame_partition!\n", rom_filename);
    std::this_thread::sleep_for(10s);
  }
  uint8_t* romdata = get_mmapped_romdata();

  fmt::print("Got mmapped romdata for {}, length={}\n", rom_filename, rom_size_bytes);

  // Clear the display
  espp::St7789::clear(0,0,320,240);
  int x_offset, y_offset;
  // store the offset for resetting to later (after emulation ends)
  espp::St7789::get_offset(x_offset, y_offset);

  switch (selected_rom_info.platform) {
  case Emulator::GAMEBOY:
  case Emulator::GAMEBOY_COLOR:
    init_gameboy(rom_filename, romdata, rom_size_bytes);
    while (!quit) {
      run_gameboy_rom();
    }
    break;
  case Emulator::NES:
    init_nes(rom_filename, display->vram0(), romdata, rom_size_bytes);
    while (!quit) {
      run_nes_rom();
    }
    break;
  default:
    break;
  }

  fmt::print("Resuming your regularly scheduled programming...\n");

  // If we got here, it's because the user sent a "quit" command
  espp::St7789::clear(0,0,320,240);
  // reset the offset
  espp::St7789::set_offset(x_offset, y_offset);
  display->resume();
  gui.resume();
  while (true) {
    std::this_thread::sleep_for(100ms);
  }
}
