idf_component_register(
  INCLUDE_DIRS "include"
  SRC_DIRS "src"
  EXCLUDE_SRCS "src/input.cpp" "src/spi_lcd.cpp"
  REQUIRES "driver" "heap" "esp_lcd" "esp_littlefs" "esp_psram" "esp_timer" "spi_flash" "nvs_flash" "codec" "display" "display_drivers" "controller" "ads1x15" "qwiicnes" 
"input_drivers" "event_manager"
  )
