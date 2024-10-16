#include "i2c.hpp"

#include <atomic>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "pindefs.h"
#include "format.hpp"

static constexpr int I2C_FREQ_HZ = (400*1000);
static const int I2C_BUS_TICKS_TO_WAIT = pdMS_TO_TICKS(10);

static std::atomic<bool> initialized = false;
void i2c_init() {
  if (initialized) return;

  esp_err_t err;

  fmt::print("initializing internal i2c driver...\n");
  i2c_config_t internal_i2c_cfg;
  internal_i2c_cfg.sda_io_num = PIN_I2C_SDA;
  internal_i2c_cfg.scl_io_num = PIN_I2C_SCL;
  internal_i2c_cfg.mode = I2C_MODE_MASTER;
  internal_i2c_cfg.sda_pullup_en = GPIO_PULLUP_DISABLE;
  internal_i2c_cfg.scl_pullup_en = GPIO_PULLUP_DISABLE;
  internal_i2c_cfg.master.clk_speed = I2C_FREQ_HZ;
  err = i2c_param_config(I2C_INTERNAL, &internal_i2c_cfg);
  if (err != ESP_OK) printf("config i2c failed\n");
  err = i2c_driver_install(I2C_INTERNAL, I2C_MODE_MASTER,  0, 0, 0); // buff len (x2), default flags
  if (err != ESP_OK) printf("install i2c driver failed\n");

  // pull reset up so i2c switch can work
  gpio_set_direction(PIN_I2C_RESET, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_I2C_RESET, 1);

  initialized = true;
}

void i2c_deinit() {
  i2c_driver_delete(I2C_INTERNAL);
}

esp_err_t i2c_read_byte(uint8_t dev_addr, uint8_t *read_data) {
  esp_err_t err;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | 0x01, I2C_MASTER_ACK);
  i2c_master_read_byte(cmd, read_data, I2C_MASTER_NACK);
  i2c_master_stop(cmd);

  err = i2c_master_cmd_begin(I2C_INTERNAL, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete(cmd);

  return err;
}

esp_err_t i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len) {
  esp_err_t err;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, dev_addr << 1, I2C_MASTER_ACK);
  i2c_master_write_byte(cmd, reg_addr, I2C_MASTER_ACK);
  i2c_master_start(cmd); // repeated start
  i2c_master_write_byte(cmd, (dev_addr << 1) | 0x01, I2C_MASTER_ACK);
  i2c_master_read(cmd, read_data, read_len, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);

  err = i2c_master_cmd_begin(I2C_INTERNAL, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete(cmd);

  return err;
}

esp_err_t i2c_write_byte(uint8_t dev_addr, uint8_t data) {
  esp_err_t err;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, dev_addr << 1, I2C_MASTER_ACK);
  i2c_master_write_byte(cmd, data, I2C_MASTER_ACK);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(I2C_INTERNAL, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete(cmd);

  return err;
}

esp_err_t i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
  esp_err_t err;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, dev_addr << 1, I2C_MASTER_ACK);
  i2c_master_write_byte(cmd, reg_addr, I2C_MASTER_ACK);
  i2c_master_write(cmd, data, len, I2C_MASTER_ACK);
  i2c_master_stop(cmd);
  err = i2c_master_cmd_begin(I2C_INTERNAL, cmd, I2C_BUS_TICKS_TO_WAIT);
  i2c_cmd_link_delete(cmd);

  return err;
}

esp_err_t i2c_switch(i2cswitch_periph_t peripheral) {
  uint8_t periph = (uint8_t)(1 << peripheral);
  //uint8_t periph = 0xff; // debug: connect all channels (can do scope debugging off a minibadge port)

  uint8_t switch_state;
  esp_err_t err;
  err = i2c_read_byte(I2C_SWITCH_ADDR, &switch_state);
  if(err) {
    printf("Error %d reading I2C switch state\n", err);
    return err;
  }
  if(!(switch_state & peripheral)) {
    // only change switch if not already set
    return i2c_write_byte(I2C_SWITCH_ADDR, periph);
  }
  return 0;  
}
