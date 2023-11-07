#include "badge_input.h"

#include "task.hpp"

#include "pindefs.h"

#define USE_QWIICNES 0
#if USE_QWIICNES
#include "i2c.hpp"
#include "qwiicnes.hpp"
#else
#include "driver/gpio.h"
#endif

using namespace std::chrono_literals;

#if USE_QWIICNES
static std::shared_ptr<QwiicNes> qwiicnes;
#endif

/**
 * Gamepad controller configuration
 */

#if USE_QWIICNES
void qwiicnes_write(uint8_t reg_addr, uint8_t value) {
  uint8_t write_buf = value;
  i2c_write_reg(QwiicNes::ADDRESS, reg_addr, &write_buf, 1);
}

uint8_t qwiicnes_read(uint8_t reg_addr) {
  uint8_t data;
  i2c_read_reg(QwiicNes::ADDRESS, reg_addr, &data, 1);
  return data;
}
#endif // else !USE_QWIICNES

static std::atomic<bool> initialized = false;
void init_input() {
  if (initialized) return;
  fmt::print("Initializing input drivers...\n");

#if USE_QWIICNES
  fmt::print("Making QwiicNES\n");
  qwiicnes = std::make_shared<QwiicNes>(QwiicNes::Config{
      .write = qwiicnes_write,
      .read = qwiicnes_read,
    });
#else
  fmt::print("initializing GPIO\n");
  gpio_set_direction(PIN_GPIO_A, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_GPIO_B, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_GPIO_START, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_GPIO_LEFT, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_GPIO_RIGHT, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_GPIO_UP, GPIO_MODE_INPUT);
  gpio_set_direction(PIN_GPIO_DOWN, GPIO_MODE_INPUT);
  gpio_pullup_en(PIN_GPIO_A);
  gpio_pullup_en(PIN_GPIO_B);
  gpio_pullup_en(PIN_GPIO_START);
  gpio_pullup_en(PIN_GPIO_LEFT);
  gpio_pullup_en(PIN_GPIO_RIGHT);
  gpio_pullup_en(PIN_GPIO_UP);
  gpio_pullup_en(PIN_GPIO_DOWN);
#endif

  initialized = true;
}

extern "C" void get_input_state(struct InputState* state) {
  bool is_a_pressed = false;
  bool is_b_pressed = false;
  bool is_x_pressed = false;
  bool is_y_pressed = false;
  bool is_select_pressed = false;
  bool is_start_pressed = false;
  bool is_up_pressed = false;
  bool is_down_pressed = false;
  bool is_left_pressed = false;
  bool is_right_pressed = false;
#if USE_QWIICNES
  if (!qwiicnes) {
    fmt::print("cannot get input state: qwiicnes not initialized properly!\n");
    return;
  }
  auto button_state = qwiicnes->read_current_state();
  is_a_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::A);
  is_b_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::B);
  is_select_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::SELECT);
  is_start_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::START);
  is_up_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::UP);
  is_down_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::DOWN);
  is_left_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::LEFT);
  is_right_pressed = QwiicNes::is_pressed(button_state, QwiicNes::Button::RIGHT);
#else
  is_a_pressed = !gpio_get_level(PIN_GPIO_A);
  is_b_pressed = !gpio_get_level(PIN_GPIO_B);
  is_start_pressed = !gpio_get_level(PIN_GPIO_START);
  is_left_pressed = !gpio_get_level(PIN_GPIO_LEFT);
  is_right_pressed = !gpio_get_level(PIN_GPIO_RIGHT);
  is_up_pressed = !gpio_get_level(PIN_GPIO_UP);
  is_down_pressed = !gpio_get_level(PIN_GPIO_DOWN);
#endif
  state->a = is_a_pressed;
  state->b = is_b_pressed;
  state->x = is_x_pressed;
  state->y = is_y_pressed;
  state->start = is_start_pressed;
  state->select = is_select_pressed;
  state->up = is_up_pressed;
  state->down = is_down_pressed;
  state->left = is_left_pressed;
  state->right = is_right_pressed;
}
