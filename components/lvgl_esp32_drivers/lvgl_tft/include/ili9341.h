#ifndef __ILI9341_H
#define __ILI9341_H

#include <stdint.h>
#include "lvgl.h"
#include "driver/spi_master.h"

typedef enum {
    LCD_TYPE_ILI = 1,
    LCD_TYPE_ST,
    LCD_TYPE_MAX,
} type_lcd_t;

void ili9341_init(void);
uint32_t lcd_get_id(spi_device_handle_t handle);
void ili9341_flush(lv_display_t *drv, const lv_area_t *area, unsigned char *color_map);
void ili9341_sleep_in(void);
void ili9341_sleep_out(void);

#endif /* __ILI9341_H */