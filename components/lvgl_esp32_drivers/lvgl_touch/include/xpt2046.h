#ifndef __XPT2046_H
#define __XPT2046_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#define XPT2046_IRQ             24
#define XPT2046_AVG             4
#define XPT2046_X_MIN           0
#define XPT2046_Y_MIN           0
#define XPT2046_X_MAX           4095
#define XPT2046_Y_MAX           4095
#define XPT2046_X_INV           0
#define XPT2046_Y_INV           0
#define XPT2046_XY_SWAP         0
#define XPT2046_TOUCH_THRESHOLD 400
#define XPT2046_TOUCH_IRQ       24
#define XPT2046_TOUCH_IRQ_PRESS 0
#define XPT2046_TOUCH_PRESS     4095

void xpt2046_init(void);
bool xpt2046_read(lv_indev_t *drv, lv_indev_data_t *data);

#endif /* __XPT2046_H */