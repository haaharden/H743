# 内存资源规划

## 内部flash：共2kb

Bootloader:    128kb  0x08000000               0x020000  
APP1：         896kb  0x08020000               0x0E0000
APP2：         896kb  0x08100000               0x0E0000  
参数区（双flag）128KB  0x081E0000 ~ 0x081FFFFF  0x020000

## 外部flash：w25q256，32mb

前16m         LVGL资源区
