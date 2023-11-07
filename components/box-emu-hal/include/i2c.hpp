#pragma once

#include "driver/i2c.h"

// Only used for the touchpad and the imu
static constexpr i2c_port_t I2C_INTERNAL = I2C_NUM_0;

void i2c_init();

#define I2C_AUDIO_ADDR  0b01101100
#define I2C_SWITCH_ADDR 0x70

typedef enum {
    //I2CSWITCH_AUDIO = 0,
    I2CSWITCH_MINIBADGE1 = 0,
    I2CSWITCH_MINIBADGE2,
    I2CSWITCH_MINIBADGE3,
    I2CSWITCH_MINIBADGE4,
    I2CSWITCH_MINIBADGE_EXT,
    I2CSWITCH_ACCELEROMETER,
    I2CSWITCH_AUDIO,
} i2cswitch_periph_t;

esp_err_t i2c_switch(i2cswitch_periph_t peripheral);
esp_err_t i2c_read_byte(uint8_t dev_addr, uint8_t *read_data);
esp_err_t i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *read_data, size_t read_len);
esp_err_t i2c_write_byte(uint8_t dev_addr, uint8_t data);
esp_err_t i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);
