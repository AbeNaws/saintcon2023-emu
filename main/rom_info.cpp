#include "rom_info.hpp"
#include "dirent.h"

std::vector<RomInfo> read_roms(const std::string& fs_path) {
  std::vector<RomInfo> infos;

  DIR *dir = opendir(fs_path.c_str());
  if (dir == NULL) {
    fmt::print("Cannot open {}!\n", fs_path);
    return infos;
  }

  struct dirent *de;
  while ((de = readdir(dir)) != NULL) {
    if (de->d_name[0] == '.') {
      // ignore hidden files
      continue;
    }

    Emulator platform = Emulator::UNKNOWN;
    if (endsWith(de->d_name, ".nes")) {
      platform = Emulator::NES;
    } else if (endsWith(de->d_name, ".gb")) {
      platform = Emulator::GAMEBOY;
    } else if (endsWith(de->d_name, ".gbc")) {
      platform = Emulator::GAMEBOY_COLOR;
    } else if (endsWith(de->d_name, ".sav")) {
      // do nothing except print we found a save
      fmt::print("Found save '{}'\n", de->d_name);
    }
    if (platform != Emulator::UNKNOWN) {
      char rom_path[32 + 10];
      strcpy(rom_path, fs_path.c_str());
      strcat(rom_path, de->d_name);
      // for each row, create rom entry
      fmt::print("Found ROM '{}' '{}'\n", de->d_name, rom_path);
      infos.emplace_back(std::string(de->d_name), std::string(rom_path), platform);
    }
  }
  return infos;
}
