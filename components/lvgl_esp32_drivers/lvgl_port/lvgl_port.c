#include "lvgl_port.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "LVGL_PORT" // 日志标签

#define LCD_WIDTH  240  // LCD宽度
#define LCD_HEIGHT 280  // LCD高度

static lv_display_t *disp_drv; // 显示驱动实例

// 函数声明
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, unsigned char *color_p);
static void lv_disp_init(void);
static void indev_read(lv_indev_t *indev_drv, lv_indev_data_t *data);
static void lv_indev_init(void);
static void lv_timer_cb(void *arg);
static void lv_tick_init(void);

/**
 * @brief 显示刷新函数（LVGL回调）
 * @param disp_drv 显示驱动实例
 * @param area 刷新区域
 * @param color_p 颜色数据指针
 */
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, unsigned char *color_p)
{
    ili9341_flush(disp_drv, area, color_p);
    lv_disp_flush_ready(disp_drv);
}

/**
 * @brief 初始化显示驱动
 */
static void lv_disp_init(void)
{
    const size_t disp_buf_size = LCD_WIDTH * (LCD_HEIGHT / 7); // 计算显示缓冲区大小

    lv_color_t *disp_buf1 = heap_caps_malloc(disp_buf_size * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lv_color_t *disp_buf2 = heap_caps_malloc(disp_buf_size * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    
    if (!disp_buf1 || !disp_buf2) {
        ESP_LOGE(TAG, "显示缓冲区分配失败！");
        return;
    }

    disp_drv = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(disp_drv, disp_flush);
    lv_display_set_buffers(disp_drv, disp_buf1, disp_buf2, disp_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    ESP_LOGI(TAG, "显示驱动初始化完成，缓冲区大小: %d bytes", (int)disp_buf_size * sizeof(lv_color_t));
}

/**
 * @brief 输入设备读取函数（LVGL回调）
 * @param indev_drv 输入设备驱动实例
 * @param data 输入数据结构体
 */
static void IRAM_ATTR indev_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    xpt2046_read(indev_drv, data);
}

/**
 * @brief 初始化输入设备（触摸屏）
 */
static void lv_indev_init(void)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read);
    ESP_LOGI(TAG, "触摸输入设备初始化完成");
}

/**
 * @brief LVGL定时器回调函数
 * @param arg 参数（tick间隔）
 */
static void lv_timer_cb(void *arg)
{
    uint32_t tick_interval = *((uint32_t*)arg);
    lv_tick_inc(tick_interval);
}

/**
 * @brief 初始化LVGL定时器
 */
static void lv_tick_init(void)
{
    static uint32_t tick_interval = 5; // tick间隔，单位ms
    const esp_timer_create_args_t timer_args = {
        .arg = &tick_interval,
        .callback = lv_timer_cb,
        .name = "lvgl_tick",
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true,
    };

    esp_timer_handle_t timer_handle;
    esp_err_t ret = esp_timer_create(&timer_args, &timer_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "定时器创建失败: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_timer_start_periodic(timer_handle, tick_interval * 1000); // 转换为微秒
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "定时器启动失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "LVGL定时器初始化完成，间隔: %lu ms", (unsigned long)tick_interval);
}

/**
 * @brief LVGL移植初始化函数
 */
void lv_port_init(void)
{
    lv_init();
    ESP_LOGI(TAG, "LVGL核心初始化完成");

    disp_spi_init_config();
    ESP_LOGI(TAG, "SPI总线初始化完成");

    ili9341_init();
    ESP_LOGI(TAG, "ILI9341显示屏初始化完成");

    xpt2046_init();
    ESP_LOGI(TAG, "XPT2046触摸屏初始化完成");

    lv_disp_init();
    lv_indev_init();
    lv_tick_init();

    ESP_LOGI(TAG, "LVGL移植初始化全部完成");
}