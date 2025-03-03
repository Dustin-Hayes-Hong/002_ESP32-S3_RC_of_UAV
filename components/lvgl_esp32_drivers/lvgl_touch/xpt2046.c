#include "xpt2046.h"
#include "touch_spi.h"
#include <stdint.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_system.h"

#define TAG "XPT2046"

#define CMD_X_READ  0b10010000
#define CMD_Y_READ  0b11010000
#define CMD_Z1_READ 0b10110000
#define CMD_Z2_READ 0b11000000

typedef enum {
    TOUCH_NOT_DETECTED = 0,
    TOUCH_DETECTED = 1,
} xpt2046_touch_detect_t;

static int16_t avg_buf_x[XPT2046_AVG];
static int16_t avg_buf_y[XPT2046_AVG];
static uint8_t avg_last;

// 函数声明
static xpt2046_touch_detect_t xpt2046_is_touch_detected(void);
static int16_t xpt2046_cmd(uint8_t cmd);
static void xpt2046_corr(int16_t *x, int16_t *y);
static void xpt2046_avg(int16_t *x, int16_t *y);

/**
 * @brief 初始化XPT2046触摸屏
 */
void xpt2046_init(void)
{
    ESP_LOGI(TAG, "开始初始化XPT2046触摸屏");

#if XPT2046_TOUCH_IRQ || XPT2046_TOUCH_IRQ_PRESS
    gpio_config_t irq_config = {
        .pin_bit_mask = BIT64(XPT2046_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&irq_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IRQ引脚配置失败: %s", esp_err_to_name(ret));
        return;
    }
#endif

    tp_spi_add_device(SPI2_HOST);
    ESP_LOGI(TAG, "XPT2046触摸屏初始化完成");
}

/**
 * @brief 读取触摸屏数据
 */
bool xpt2046_read(lv_indev_t *drv, lv_indev_data_t *data)
{
    static int16_t last_x = 0;
    static int16_t last_y = 0;
    bool valid = false;

    int16_t x = last_x;
    int16_t y = last_y;

    if (xpt2046_is_touch_detected() == TOUCH_DETECTED) {
        valid = true;

        x = xpt2046_cmd(CMD_X_READ);
        y = xpt2046_cmd(CMD_Y_READ);
        ESP_LOGD(TAG, "原始坐标: P(%d, %d)", x, y);

        x = x >> 4;
        y = y >> 4;
        ESP_LOGD(TAG, "规范化坐标: P_norm(%d, %d)", x, y);

        xpt2046_corr(&x, &y);
        xpt2046_avg(&x, &y);
        last_x = x;
        last_y = y;

        ESP_LOGI(TAG, "校正后坐标: x = %d, y = %d", x, y);
    } else {
        avg_last = 0;
    }

    data->point.x = x;
    data->point.y = y;
    data->state = valid ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    return false;
}

/**
 * @brief 检测触摸状态
 */
static xpt2046_touch_detect_t xpt2046_is_touch_detected(void)
{
#if XPT2046_TOUCH_IRQ || XPT2046_TOUCH_IRQ_PRESS
    uint8_t irq = gpio_get_level(XPT2046_IRQ);
    if (irq != 0) {
        return TOUCH_NOT_DETECTED;
    }
#endif

#if XPT2046_TOUCH_PRESS || XPT2046_TOUCH_IRQ_PRESS
    int16_t z1 = xpt2046_cmd(CMD_Z1_READ) >> 3;
    int16_t z2 = xpt2046_cmd(CMD_Z2_READ) >> 3;
    int16_t z = z1 + 4096 - z2;

    if (z < XPT2046_TOUCH_THRESHOLD) {
        return TOUCH_NOT_DETECTED;
    }
#endif

    return TOUCH_DETECTED;
}

/**
 * @brief 发送命令并读取数据
 */
static int16_t xpt2046_cmd(uint8_t cmd)
{
    uint8_t data[2];
    tp_spi_read_reg(cmd, data, 2);
    return (data[0] << 8) | data[1];
}

/**
 * @brief 校正触摸坐标
 */
static void xpt2046_corr(int16_t *x, int16_t *y)
{
#if XPT2046_XY_SWAP != 0
    int16_t swap_tmp = *x;
    *x = *y;
    *y = swap_tmp;
#endif

    if (*x > XPT2046_X_MIN) *x -= XPT2046_X_MIN;
    else *x = 0;

    if (*y > XPT2046_Y_MIN) *y -= XPT2046_Y_MIN;
    else *y = 0;

    *x = (uint32_t)((uint32_t)(*x) * LV_HOR_RES) / (XPT2046_X_MAX - XPT2046_X_MIN);
    *y = (uint32_t)((uint32_t)(*y) * LV_VER_RES) / (XPT2046_Y_MAX - XPT2046_Y_MIN);

#if XPT2046_X_INV != 0
    *x = LV_HOR_RES - *x;
#endif

#if XPT2046_Y_INV != 0
    *y = LV_VER_RES - *y;
#endif
}

/**
 * @brief 计算触摸坐标平均值
 */
static void xpt2046_avg(int16_t *x, int16_t *y)
{
    for (uint8_t i = XPT2046_AVG - 1; i > 0; i--) {
        avg_buf_x[i] = avg_buf_x[i - 1];
        avg_buf_y[i] = avg_buf_y[i - 1];
    }

    avg_buf_x[0] = *x;
    avg_buf_y[0] = *y;
    if (avg_last < XPT2046_AVG) avg_last++;

    int32_t x_sum = 0, y_sum = 0;
    for (uint8_t i = 0; i < avg_last; i++) {
        x_sum += avg_buf_x[i];
        y_sum += avg_buf_y[i];
    }

    *x = x_sum / avg_last;
    *y = y_sum / avg_last;
}