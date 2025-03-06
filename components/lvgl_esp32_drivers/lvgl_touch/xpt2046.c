#include "xpt2046.h"
#include "touch_spi.h"      // 触摸屏SPI驱动头文件
#include <stdint.h>
#include "esp_log.h"        // ESP-IDF日志库
#include "driver/gpio.h"    // GPIO驱动头文件
#include "esp_system.h"     // ESP系统函数

// 日志标签
static const char *TAG = "XPT2046";

// XPT2046命令定义
#define CMD_X_READ  0b10010000  // 读取X坐标命令
#define CMD_Y_READ  0b11010000  // 读取Y坐标命令
#define CMD_Z1_READ 0b10110000  // 读取Z1压力值命令
#define CMD_Z2_READ 0b11000000  // 读取Z2压力值命令

// 触摸检测状态枚举
typedef enum {
    TOUCH_NOT_DETECTED = 0,     // 未检测到触摸
    TOUCH_DETECTED = 1,         // 检测到触摸
} xpt2046_touch_detect_t;

// 静态变量，用于坐标平均计算
static int16_t avg_buf_x[XPT2046_AVG];  // X坐标平均缓冲区
static int16_t avg_buf_y[XPT2046_AVG];  // Y坐标平均缓冲区
static uint8_t avg_last;                // 当前平均值计数

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
    // 配置IRQ引脚
    gpio_config_t irq_config = {
        .pin_bit_mask = BIT64(XPT2046_IRQ),  // 指定IRQ引脚
        .mode = GPIO_MODE_INPUT,             // 输入模式
        .pull_up_en = GPIO_PULLUP_DISABLE,   // 不启用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 不启用下拉
        .intr_type = GPIO_INTR_DISABLE,      // 禁用中断
    };

    esp_err_t ret = gpio_config(&irq_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IRQ引脚配置失败: %s", esp_err_to_name(ret));
        return;
    }
#endif

    avg_last = 0;  // 初始化平均计数
    ESP_LOGI(TAG, "XPT2046触摸屏初始化完成");
}

/**
 * @brief 读取触摸屏数据（LVGL回调）
 * @param drv LVGL输入设备驱动结构体指针
 * @param data 触摸数据结构体指针
 * @return 是否有未处理的数据（始终返回false）
 */
bool xpt2046_read(lv_indev_t *drv, lv_indev_data_t *data)
{
    static int16_t last_x = 0;  // 上次X坐标
    static int16_t last_y = 0;  // 上次Y坐标
    bool valid = false;         // 数据有效性标志

    int16_t x = last_x;
    int16_t y = last_y;

    if (xpt2046_is_touch_detected() == TOUCH_DETECTED) 
    {
        valid = true;

        // 读取原始坐标
        x = xpt2046_cmd(CMD_X_READ);
        y = xpt2046_cmd(CMD_Y_READ);
        ESP_LOGD(TAG, "原始坐标: P(%d, %d)", x, y);

        // 规范化坐标（右移4位，去除低位噪声）
        x = x >> 4;
        y = y >> 4;
        ESP_LOGD(TAG, "规范化坐标: P_norm(%d, %d)", x, y);

        // 校正和平均处理
        xpt2046_corr(&x, &y);
        xpt2046_avg(&x, &y);
        last_x = x;
        last_y = y;

        ESP_LOGI(TAG, "校正后坐标: x = %d, y = %d", x, y);
    } else {
        avg_last = 0;  // 无触摸时重置平均计数
    }

    // 更新LVGL数据
    data->point.x = x;
    data->point.y = y;
    data->state = valid == false ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    return false;  // 无需缓冲数据
}

/**
 * @brief 检测触摸状态
 * @return 触摸检测结果
 */
static xpt2046_touch_detect_t xpt2046_is_touch_detected(void)
{
#if XPT2046_TOUCH_IRQ || XPT2046_TOUCH_IRQ_PRESS
    // 使用IRQ引脚检测触摸
    uint8_t irq = gpio_get_level(XPT2046_IRQ);
    if (irq != 0) {
        return TOUCH_NOT_DETECTED;  // IRQ高电平表示无触摸
    }
#endif

#if XPT2046_TOUCH_PRESS || XPT2046_TOUCH_IRQ_PRESS
    // 使用压力值检测触摸
    int16_t z1 = xpt2046_cmd(CMD_Z1_READ) >> 3;
    int16_t z2 = xpt2046_cmd(CMD_Z2_READ) >> 3;
    int16_t z = z1 + 4096 - z2;  // 计算压力值

    ESP_LOGD(TAG, "压力值: z1=%d, z2=%d, z=%d", z1, z2, z);
    if (z < XPT2046_TOUCH_THRESHOLD) {
        return TOUCH_NOT_DETECTED;  // 压力低于阈值
    }
#endif
    return TOUCH_DETECTED;
}

/**
 * @brief 发送命令并读取数据
 * @param cmd SPI命令字节
 * @return 16位触摸数据
 */
static int16_t xpt2046_cmd(uint8_t cmd)
{
    uint8_t data[2];
    tp_spi_read_reg(cmd, data, 2);
    return (int16_t)((data[0] << 8) | data[1]);  // 合并高低字节
}

/**
 * @brief 校正触摸坐标
 * @param x X坐标指针
 * @param y Y坐标指针
 */
static void xpt2046_corr(int16_t *x, int16_t *y)
{
#if XPT2046_XY_SWAP != 0
    // 交换X和Y坐标
    int16_t swap_tmp = *x;
    *x = *y;
    *y = swap_tmp;
#endif

    // 去除最小值偏移
    *x = (*x > XPT2046_X_MIN) ? (*x - XPT2046_X_MIN) : 0;
    *y = (*y > XPT2046_Y_MIN) ? (*y - XPT2046_Y_MIN) : 0;

    // 映射到显示分辨率
    *x = (int16_t)((int32_t)(*x) * LV_HOR_RES) / (XPT2046_X_MAX - XPT2046_X_MIN);
    *y = (int16_t)((int32_t)(*y) * LV_VER_RES) / (XPT2046_Y_MAX - XPT2046_Y_MIN);

#if XPT2046_X_INV != 0
    *x = LV_HOR_RES - *x;  // 反转X轴
#endif

#if XPT2046_Y_INV != 0
    *y = LV_VER_RES - *y;  // 反转Y轴
#endif
}

/**
 * @brief 计算触摸坐标平均值以减少噪声
 * @param x X坐标指针
 * @param y Y坐标指针
 */
static void xpt2046_avg(int16_t *x, int16_t *y)
{
    // 移位缓冲区
    for (uint8_t i = XPT2046_AVG - 1; i > 0; i--) {
        avg_buf_x[i] = avg_buf_x[i - 1];
        avg_buf_y[i] = avg_buf_y[i - 1];
    }

    // 添加新数据
    avg_buf_x[0] = *x;
    avg_buf_y[0] = *y;
    if (avg_last < XPT2046_AVG) avg_last++;

    // 计算平均值
    int32_t x_sum = 0, y_sum = 0;
    for (uint8_t i = 0; i < avg_last; i++) {
        x_sum += avg_buf_x[i];
        y_sum += avg_buf_y[i];
    }

    *x = (int16_t)(x_sum / avg_last);
    *y = (int16_t)(y_sum / avg_last);
}