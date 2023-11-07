#include "i2s_audio.h"

#include <atomic>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"

#include "esp_system.h"
#include "esp_check.h"

#include "i2c.hpp"
#include "task.hpp"
#include "event_manager.hpp"

#include "hal_events.hpp"

#include "pindefs.h"

/**
 * Look at
 * https://github.com/espressif/esp-idf/blob/master/examples/peripherals/i2s/i2s_codec/i2s_es8311/main/i2s_es8311_example.c
 * and
 * https://github.com/espressif/esp-box/blob/master/components/bsp/src/peripherals/bsp_i2s.c
 */

// This board uses a TFA9879 amplifier

/* Example configurations */
#define EXAMPLE_MCLK_MULTIPLE   (I2S_MCLK_MULTIPLE_256) // If not using 24-bit data width, 256 should be enough
#define EXAMPLE_MCLK_FREQ_HZ    (AUDIO_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define EXAMPLE_VOLUME          (60) // percent

static i2s_chan_handle_t tx_handle = NULL;

static int16_t *audio_buffer;
static uint8_t amp_read[2];

static std::atomic<bool> muted_{false};
static std::atomic<int> volume_{60};

int16_t *get_audio_buffer() {
  return audio_buffer;
}

void update_volume_output() {
  esp_err_t err;
  err = i2c_switch(I2CSWITCH_AUDIO);
  uint8_t i2c_write[2];
  i2c_write[0] = 0x10;
  if (muted_) {
    i2c_write[1] = 0xbd;
  } else {
    i2c_write[1] = (uint8_t)((100 - volume_) * 2);
  }
  err |= i2c_write_reg(I2C_AUDIO_ADDR, 0x13, i2c_write, 2);
}

void set_muted(bool mute) {
  muted_ = mute;
  update_volume_output();
}

bool is_muted() {
  return muted_;
}

void set_audio_volume(int percent) {
  if(percent > 100 || percent < 0) return;
  volume_ = percent;
  update_volume_output();
}

int get_audio_volume() {
  return volume_;
}

static esp_err_t i2s_driver_init(void)
{
  printf("initializing i2s driver...\n");
  auto ret_val = ESP_OK;
  printf("Using newer I2S standard\n");
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));
  i2s_std_clk_config_t clock_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE);
  i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  i2s_std_config_t std_cfg = {
    .clk_cfg = clock_cfg,
    .slot_cfg = slot_cfg,
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = PIN_AUDIO_SCK1,
      .ws = PIN_AUDIO_LRCK1,
      .dout = PIN_AUDIO_SDI1,
      .din = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
  return ret_val;
}

static bool initialized = false;
void audio_init() {
  if (initialized) return;
  //espp::EventManager::get().add_publisher(mute_button_topic, "i2s_audio");

  i2s_driver_init();

  // wake audio amplifier
  esp_err_t err = i2c_switch(I2CSWITCH_AUDIO);

  // set POWERUP
  uint8_t i2c_write[2];
  i2c_write[0] = 0x00;
  i2c_write[1] = 0x01;
  err |= i2c_write_reg(I2C_AUDIO_ADDR, 0x00, i2c_write, 2);

  // set OPMODE 1 and POWERUP 1, and I2S_FS (sample rate)
  uint8_t bigwrite[4];
  bigwrite[0] = 0x00;
  bigwrite[1] = 0x09;
  // serial interface control: 0b0000 1001 1001 1000
  bigwrite[2] = 0x09;
  bigwrite[3] = 0x98;
  err |= i2c_write_reg(I2C_AUDIO_ADDR, 0x00, bigwrite, 4);

  audio_buffer = (int16_t*)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
  initialized = true;
}

void audio_deinit() {
  if (!initialized) return;
  i2s_channel_disable(tx_handle);
  i2s_del_channel(tx_handle);

  // set POWERDOWN
  uint8_t i2c_write[2];
  i2c_write[0] = 0x00;
  i2c_write[1] = 0x00;
  i2c_write_reg(I2C_AUDIO_ADDR, 0x00, i2c_write, 2);
  initialized = false;
}

void audio_play_frame(uint8_t *data, uint32_t num_bytes) {
  size_t bytes_written = 0;
  auto err = ESP_OK;
  err = i2s_channel_write(tx_handle, data, num_bytes, &bytes_written, 1000);
  if(num_bytes != bytes_written) {
    printf("ERROR to write %ld != written %d\n", num_bytes, bytes_written);
  }
  if (err != ESP_OK) {
    printf("ERROR writing i2s channel: %d, '%s'\n", err, esp_err_to_name(err));
  }
}
