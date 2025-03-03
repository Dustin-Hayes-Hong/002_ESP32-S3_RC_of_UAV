#include "lvgl_port.h"
#include "disp_spi.h"
#include "ili9341.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "xpt2046.h"
#include "esp_timer.h"

#define TAG     "lv_port"

#define LCD_WIDTH       240
#define LCD_HEIGHT      280

static lv_display_t *disp_drv;

void lv_flush_done_cb(void* param)
{
    lv_disp_flush_ready(disp_drv);
}

void disp_flush(lv_display_t *disp_drv, const lv_area_t * area, unsigned char * color_p)
{
    ili9341_flush(disp_drv,area,color_p);
    lv_disp_flush_ready(disp_drv);
}

void lv_disp_init(void)
{
    const size_t disp_buf_size = LCD_WIDTH*(LCD_HEIGHT/7);

    lv_color_t *disp1 = heap_caps_malloc(disp_buf_size*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    lv_color_t *disp2 = heap_caps_malloc(disp_buf_size*sizeof(lv_color_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    
    disp_drv = lv_display_create(LCD_WIDTH,LCD_HEIGHT);
    lv_display_set_flush_cb(disp_drv, disp_flush);
    lv_display_set_buffers(disp_drv, disp1, disp2, disp_buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void IRAM_ATTR indev_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
    xpt2046_read(indev_drv,data);
}

void lv_indev_init(void)
{
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read);
}

void lv_timer_cb(void* arg)
{
    uint32_t tick_interval = *((uint32_t*)arg);
    lv_tick_inc(tick_interval);
}

void lv_tick_init(void)
{
    static uint32_t tick_interval = 5;
    const esp_timer_create_args_t arg = 
    {
        .arg = &tick_interval,
        .callback = lv_timer_cb,
        .name = "",
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true,
    };

    esp_timer_handle_t timer_handle;
    esp_timer_create(&arg,&timer_handle);
    esp_timer_start_periodic(timer_handle,tick_interval*1000);
}

void lv_port_init(void)
{
    lv_init();
    ili9341_init();
    xpt2046_init();
    lv_disp_init();
    lv_indev_init();
    lv_tick_init();
}