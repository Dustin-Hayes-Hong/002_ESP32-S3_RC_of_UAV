# 介绍 

开发工具VSCode  ESP-IDF，编译环境为esp-idf v5.4

LVGL移植[教程](https://blog.csdn.net/2401_84036568/article/details/144636538)

~~LVGL选择[v9.2.2](https://github.com/lvgl/lvgl/releases/tag/v9.2.2)，[视频移植过程](https://www.bilibili.com/video/BV1cD4y1P71y/)，[文字移植过程](https://github.com/lvgl/lv_port_esp32)，[esp32的LVGL驱动](https://github.com/lvgl/lvgl_esp32_drivers)~~

也可参考[官方移植示例](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/lcd/i80_controller)和[官方触摸示例](https://github.com/espressif/esp-idf/tree/v5.4/examples/peripherals/lcd/spi_lcd_touch)。

## 如何使用

在VSCODE安装ESP-IDF，并安装ESP-IDF v5.4.0

以下是项目文件夹中其余文件的简短说明：

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  这是您当前正在阅读的文件
```

## 相关资料

- [ESP32运行LVGLv9.2](https://blog.csdn.net/2401_84036568/article/details/144636538)

