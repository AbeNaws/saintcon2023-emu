#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "display.hpp"
#include "task.hpp"

#include "pindefs.h"
#include "i80_lcd.h"

static const char *TAG = "lcd";
std::shared_ptr<espp::Display> display;

// Multiples of 9 appear to work for this display
#define LCD_PIXEL_CLOCK_HZ     (18 * 1000 * 1000)

// The pixel number in horizontal and vertical
#define LCD_H_RES              320
#define LCD_V_RES              240
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define LVGL_TICK_PERIOD_MS    2

// Supported alignment: 16, 32, 64. A higher alignment can enables higher burst transfer size, thus a higher i80 bus throughput.
#define PSRAM_DATA_ALIGNMENT   64

static lv_disp_drv_t disp_drv;      // contains callback functions
esp_lcd_panel_handle_t panel_handle = NULL;

// alloc draw buffers used by LVGL
// it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
static constexpr size_t vram_buffer_size = LCD_H_RES * NUM_ROWS_IN_FRAME_BUFFER;
static constexpr size_t max_transfer_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);

// alloc frame buffers used by emulators
static constexpr size_t frame_buffer_size = LCD_H_RES * LCD_V_RES * 2;
static uint8_t *frame_buffer0;
static uint8_t *frame_buffer1;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    uint16_t offsetx1 = area->x1;
    uint16_t offsetx2 = area->x2;
    uint16_t offsety1 = area->y1;
    uint16_t offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

void display_clear() {
    // TODO make this work properly
    static uint16_t color_data[max_transfer_size / 8];
    memset(color_data, 0x0000, max_transfer_size / 8 * sizeof(uint16_t));
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, (lv_color_t*)color_data);
}

extern "C" uint16_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return lv_color_make(r,g,b).full;
}

extern "C" uint16_t* get_vram0() {
    return display->vram0();
}

extern "C" uint16_t* get_vram1() {
    return display->vram1();
}

extern "C" uint8_t* get_frame_buffer0() {
    return frame_buffer0;
}

extern "C" uint8_t* get_frame_buffer1() {
    return frame_buffer1;
}

extern "C" void lcd_write_frame(const uint16_t xs, const uint16_t ys, const uint16_t width, const uint16_t height, const uint8_t * data){
    if(data) {
        //esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) disp_drv.user_data;
        //esp_lcd_panel_draw_bitmap(panel_handle, xs, ys, xs + width, ys + height, (lv_color_t*)data);
        esp_lcd_panel_draw_bitmap(panel_handle, xs, ys, xs + width, ys + height, (lv_color_t*)data);
    }
}

static bool initialized = false;
extern "C" void lcd_init() {
    if (initialized) {
        return;
    }

    ESP_LOGI(TAG, "Turn off LCD backlight");
    // backlight
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .gpio_num = PIN_DISP_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0   
    };
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = PIN_DISP_DC,
        .wr_gpio_num = PIN_DISP_PCLK,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            PIN_DISP_DATA0,
            PIN_DISP_DATA1,
            PIN_DISP_DATA2,
            PIN_DISP_DATA3,
            PIN_DISP_DATA4,
            PIN_DISP_DATA5,
            PIN_DISP_DATA6,
            PIN_DISP_DATA7,
        },
        .bus_width = 8,
        .max_transfer_bytes = max_transfer_size,
        .psram_trans_align = PSRAM_DATA_ALIGNMENT,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = PIN_DISP_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = !LV_COLOR_16_SWAP, // Swap can be done in LvGL (default) or DMA
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install LCD driver of st7789");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_DISP_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    // Set inversion, x/y coordinate order, x/y mirror according to your LCD module spec
    // the gap is LCD panel specific, even panels with the same driver IC, can have different gap value
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_set_gap(panel_handle, 0, 0);

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, true);

    ESP_LOGI(TAG, "Initialize LVGL library");
    using namespace std::chrono_literals;
    display = std::make_shared<espp::Display>(espp::Display::AllocatingConfig{
            .width = LCD_H_RES,
            .height = LCD_V_RES,
            .pixel_buffer_size = vram_buffer_size,
            .flush_callback = lvgl_flush_cb,
            .update_period = 5ms,
            .double_buffered = true,
            .allocation_flags = MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL,
            .rotation = espp::Display::Rotation::LANDSCAPE,
            .software_rotation_enabled = true,
            .log_level = espp::Logger::Verbosity::WARN,
        });

    frame_buffer0 = (uint8_t*)heap_caps_malloc(frame_buffer_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    frame_buffer1 = (uint8_t*)heap_caps_malloc(frame_buffer_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "Turn on LCD backlight");
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 4095);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    initialized = true;
}
