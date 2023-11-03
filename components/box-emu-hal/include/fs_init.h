#pragma once

#include <esp_err.h>
#include <sys/stat.h>
#include <errno.h>

#if CONFIG_ROM_STORAGE_LITTLEFS
#include "esp_littlefs.h"
#define MOUNT_POINT "/littlefs"
#define SAVE_DIR "/saves"
#elif CONFIG_ROM_STORAGE_SDCARD
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#define MOUNT_POINT "/sdcard"
#define SAVE_DIR "/sdcard/saves"
#endif

void fs_init();
