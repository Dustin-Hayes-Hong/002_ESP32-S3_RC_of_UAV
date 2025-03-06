#ifndef __LVGL_PORT_H
#define __LVGL_PORT_H

#include <stdint.h>
#include "lvgl.h"
#include "disp_spi.h"
#include "ili9341.h"
#include "touch_spi.h"
#include "xpt2046.h"

void lv_port_init(void); // LVGL移植初始化函数

#endif /* __LVGL_PORT_H */