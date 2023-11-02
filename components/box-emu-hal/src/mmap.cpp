#include "mmap.hpp"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_psram.h"

const esp_partition_t* cart_partition;
static uint8_t* romdata = nullptr;

void init_memory() {
  // ROM allocation happens later
  cart_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
  if (!cart_partition) {
    fmt::print(fg(fmt::color::red), "Couldn't find cart_partition!\n");
  }
}

size_t copy_romdata_to_cart_partition(const std::string& rom_filename) {
  // load the file data and iteratively copy it over
  std::ifstream romfile(rom_filename, std::ios::binary | std::ios::ate); //open file at end
  if (!romfile.is_open()) {
    fmt::print("Error: ROM file does not exist\n");
    return 0;
  }
  // allocate memory for the ROM and make sure it's on the SPIRAM
  size_t filesize = romfile.tellg(); // get size from current file pointer location;
  fmt::print("Allocated {} bytes for ROM\n", filesize);
  romdata = (uint8_t*)heap_caps_malloc(filesize, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (romdata == nullptr) {
      fmt::print(fg(fmt::terminal_color::red), "ERROR: Couldn't allocate memory for ROM!\n");
      return 0;
  }
  romfile.seekg(0, std::ios::beg); //reset file pointer to beginning;
  romfile.read((char*)(romdata), filesize);
  romfile.close();
  return filesize;
}

extern "C" uint8_t *osd_getromdata() {
  return get_mmapped_romdata();
}

uint8_t *get_mmapped_romdata() {
  fmt::print("Initialized. ROM@{}\n", fmt::ptr(romdata));
  return romdata;
}
