#ifndef __ILI9341_H
#define __ILI9341_H

#include <stdint.h>
#include "lvgl.h"          // LVGL图形库头文件
#include "driver/spi_master.h" // ESP-IDF SPI驱动头文件

// 函数声明

/**
 * @brief 初始化ILI9341显示屏
 */
void ili9341_init(void);

/**
 * @brief 刷新显示区域（LVGL显示回调函数）
 * @param drv LVGL显示驱动结构体指针
 * @param area 需要刷新的区域
 * @param color_map 颜色数据缓冲区
 */
void ili9341_flush(lv_display_t *drv, const lv_area_t *area, unsigned char *color_map);

/**
 * @brief 使ILI9341进入睡眠模式
 */
void ili9341_sleep_in(void);

/**
 * @brief 使ILI9341退出睡眠模式
 */
void ili9341_sleep_out(void);

#endif /* __ILI9341_H */