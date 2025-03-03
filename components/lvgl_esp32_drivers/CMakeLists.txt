idf_component_register(
    SRCS 
    "lvgl_tft/ili9341.c"    "lvgl_tft/disp_spi.c"    "lvgl_port/lvgl_port.c"     "lvgl_touch/xpt2046.c"  "lvgl_touch/touch_spi.c"
    
    INCLUDE_DIRS
    "lvgl_tft/include"      "lvgl_port/include"       "lvgl_touch/include"
                    
    REQUIRES
    lvgl    esp_driver_spi   esp_driver_gpio
)