idf_component_register(
    SRCS 
    "lvgl_spi/disp_spi.c"            
    
    INCLUDE_DIRS
    "lvgl_spi/include"
                    
    REQUIRES
    lvgl    esp_driver_spi
)