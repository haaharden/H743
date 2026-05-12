#include <stdio.h>
#include <string.h>

#include "fatfs.h"
#include "usb_host.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "lv_port_flash_fs.h"
#include "lvgl.h"
#include "cmsis_os2.h"

#include "initial.h"
#include "sdram_w9825g6.h"
#include "flash_w25q256.h"
#include "log.h"

static void Initial(void)
{
    /*
  lv_fs_file_t file;
  lv_img_header_t header;
  volatile lv_fs_res_t fs_res = LV_FS_RES_UNKNOWN; // LVGL 的文件系统 API 返回值，断点时先看它是不是 LV_FS_RES_OK。
  volatile lv_res_t res = LV_RES_INV; // LVGL 的返回值，断点时先看它是不是 LV_RES_OK。*/
  BSP_SDRAM_Init();
  W25Q256_Init();
  MX_FATFS_Init();
  MX_USB_HOST_Init();

  lv_init();
  lv_port_disp_init();
  lv_port_indev_init();
  lv_port_flash_fs_init();
  LOGI("Init successful.");

  /*
  LOGI("test start");
    fs_res = lv_fs_open(&file, "R:/chaiqvan.bin", LV_FS_MODE_RD);
  if(fs_res == LV_FS_RES_OK) {
    lv_fs_close(&file);
    LOGI("File opened successfully.");
  }
  res = lv_img_decoder_get_info("R:/chaiqvan.bin", &header);
  if(res == LV_RES_OK) {
    LOGI("Image info retrieved successfully: width=%d, height=%d, color_format=%d", header.w, header.h, header.cf);
  }*/
  //osDelay(1); // 在这里打断点，直接观察本函数里的局部变量即可。
}

extern osSemaphoreId_t LvglReadySemHandle;
void Initial_Task(void *argument)
{
  Initial();
  osSemaphoreRelease(LvglReadySemHandle);
  osThreadExit();
}

/*
void test_host(void)
{
    FIL file;
    static volatile FRESULT frd;
    static volatile UINT br;
    static volatile UINT bw;

    char buf[32] = {0};
    char wbuf[] = "123459876\r\n";

    // 1. 以读写方式打开，不存在就创建，存在就清空
    frd = f_open(&file, "0:/test.txt", FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
    if (frd != FR_OK) {
        return;
    }

    // 2. 写入数据
    frd = f_write(&file, wbuf, strlen(wbuf), &bw);
    if (frd != FR_OK || bw != strlen(wbuf)) {
        f_close(&file);
        return;
    }

    // 3. 同步到U盘，测试时建议加
    frd = f_sync(&file);
    if (frd != FR_OK) {
        f_close(&file);
        return;
    }

    // 4. 文件指针回到开头
    frd = f_lseek(&file, 0);
    if (frd != FR_OK) {
        f_close(&file);
        return;
    }

    // 5. 读出来
    memset(buf, 0, sizeof(buf));

    frd = f_read(&file, buf, sizeof(buf) - 1, &br);
    if (frd == FR_OK) {
        buf[br] = '\0';

    // 打断点看：bw 应该是 11,br 应该是 11,buf 应该是 "123456789\r\n"
    }

    f_close(&file);
}
void test_list_usb_root_debug(void)
{
    static volatile FRESULT fr;
    static volatile DIR dir;
    static volatile FILINFO fno;
    static volatile unsigned int file_count;
    static volatile unsigned int dir_count;

    file_count = 0;
    dir_count = 0;

    fr = f_opendir(&dir, "0:/");
    if (fr != FR_OK) {
        return;
    }

    while (1) {
        fr = f_readdir(&dir, &fno);

        if (fr != FR_OK) {
            break;
        }

        if (fno.fname[0] == '\0') {
            break;
        }

        if (fno.fattrib & AM_DIR) {
            dir_count++;
        } else {
            file_count++;
        }

        //在这里打断点看：fno.fname、fno.fsize、fno.fattrib、file_count、dir_count
    }

    f_closedir(&dir);
}*/
/*void mount_usb_host(void)
{
  static uint8_t userh_mounted = 0;
	static volatile FRESULT fr;
  if (Appli_state == APPLICATION_READY && userh_mounted == 0) {
      fr = f_mount(&USERHFatFS, USERHPath, 1);
      if (fr == FR_OK) {
          userh_mounted = 1;
          //test_list_usb_root_debug();
          //test_host();
      }
      
  }

  if (Appli_state == APPLICATION_DISCONNECT && userh_mounted) {
      f_mount(NULL, USERHPath, 0);
      userh_mounted = 0;
  }
}*/