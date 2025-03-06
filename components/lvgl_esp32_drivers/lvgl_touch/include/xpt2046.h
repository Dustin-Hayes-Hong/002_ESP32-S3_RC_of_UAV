#ifndef __XPT2046_H
#define __XPT2046_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"  // LVGL图形库头文件

// XPT2046触摸屏配置宏
#define XPT2046_IRQ             TP_SPI_IRQ      // 中断引脚（IRQ）
#define XPT2046_AVG             4       // 坐标平均采样次数
#define XPT2046_X_MIN           200     // X轴最小原始值
#define XPT2046_Y_MIN           200     // Y轴最小原始值
#define XPT2046_X_MAX           1900    // X轴最大原始值
#define XPT2046_Y_MAX           1900    // Y轴最大原始值
#define XPT2046_X_INV           0       // 是否反转X轴（0:否, 1:是）
#define XPT2046_Y_INV           0       // 是否反转Y轴（0:否, 1:是）
#define XPT2046_XY_SWAP         0       // 是否交换XY轴（0:否, 1:是）
#define XPT2046_TOUCH_THRESHOLD 400     // 触摸压力阈值
#define XPT2046_TOUCH_IRQ       1      // 是否使用IRQ中断（0:否, 1:是）
#define XPT2046_TOUCH_IRQ_PRESS 0       // 是否使用IRQ检测触摸压力（0:否, 1:是）
#define XPT2046_TOUCH_PRESS     0    // 触摸压力检测值

/**
 * @brief 初始化XPT2046触摸屏
 */
void xpt2046_init(void);

/**
 * @brief 读取触摸屏数据（LVGL输入设备回调）
 * @param drv LVGL输入设备驱动结构体指针
 * @param data 触摸数据结构体指针
 * @return 是否有未处理的数据（始终返回false）
 */
bool xpt2046_read(lv_indev_t *drv, lv_indev_data_t *data);

#endif /* __XPT2046_H */