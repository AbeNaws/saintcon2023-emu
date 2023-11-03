#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "format.hpp"
#include "string_utils.hpp"

enum class Emulator { UNKNOWN, NES, GAMEBOY, GAMEBOY_COLOR, SEGA_MASTER_SYSTEM, GENESIS, SNES };

struct RomInfo {
  std::string name;
  std::string rom_path;
  Emulator platform;
};

std::vector<RomInfo> read_roms(const std::string& fs_path);
