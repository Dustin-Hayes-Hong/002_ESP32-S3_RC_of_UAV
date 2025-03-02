idf_component_register(
    SRCS 
    "lvgl_spi/disp_spi.c"   "lvgl_tft/ili9341.c"
    
    INCLUDE_DIRS
    "lvgl_spi/include"      "lvgl_tft/include"
                    
    REQUIRES
    lvgl    esp_driver_spi   esp_driver_gpio
)