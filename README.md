# SAINTCON 2023 Badge Emulator

This project puts retro game emulation onto your SAINTCON 2023 badge!

This uses the [esp-box-emu project](https://github.com/esp-cpp/esp-box-emu) as a basis with the following modifications:
- Removed touchscreen support
- Removed rumble support
- Removed box art support (sorry!)
- Changed display connection (ESP32-BOX-S3 uses SPI, badge uses I8080 parallel)
- Changed SD card loads to LittleFS (on-Flash storage)
- Changed ROM detection to scan for files, instead of manually populating a CSV
- Changed partition table - there is 1MB for saves, 4MB for ROMs. The ROM partition could be expanded to 8MB by editing `partitions.csv`

## Building

You will need ESP-IDF 5.0 or newer to build this project.

1. Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. Load the ESP-IDF toolchain: `. <esp-idf installation>/source.sh`
3. Clone this repository and enter the project: `git clone <url> && cd saintcon2023-emu`
4. Clone all submodules: `git submodule update --init --recursive`
5. Build the project: `idf.py build`
6. Place ROMs in the `flash_data` directory. They need to have `.nes` extensions for NES games, `.gb` for GB and `.gbc` for GBC.
7. Connect your badge and flash it: `idf.py flash`
8. Enjoy!

The D-pad click is mapped to the start button. There is no select button mapped at this time.

## Development

Badge pins have been reversed engineered and documented in `pinouts.txt`.

## Known issues

- NES games have not been tested yet.
- GBC games larger than 1MB are not yet working properly. There is only 2MB of SPI RAM, and because this loads the ROM into memory before playing it does not fit with the existing structures in that bank.
- Saves for GB/GBC games may not work consistently. The current method is to copy SRAM (aka External RAM) to a LittleFS save partition whenever SRAM writes are disabled and bank 1+ of SRAM has writes. I have tested saves on one or two games but your mileage may vary.

### TODO
- [ ] Sound
- [ ] NES support
- [ ] More expansive GB/GBC save testing
- [ ] 2MB ROM support
- [ ] Savestates
- [ ] Badge LEDs
  - [ ] Low battery LED
  - [ ] Minibadge blinking
- [ ] External controllers (through I2C?)
