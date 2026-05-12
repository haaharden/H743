/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    userh_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "usb_host.h"
#include "usbh_msc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

// USER 盘固定映射到已经验证可用的 USB Host MSC 的 LUN0。
#define USERH_MSC_LUN 0U

// 这些符号由现有 USB Host 模块提供，这里只做外部引用，不改 Host 模块本身。
extern USBH_HandleTypeDef hUsbHostHS;
extern ApplicationTypeDef Appli_state;

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USERH_initialize (BYTE pdrv);
DSTATUS USERH_status (BYTE pdrv);
DRESULT USERH_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USERH_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USERH_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USERH_Driver =
{
  USERH_initialize,
  USERH_status,
  USERH_read,
#if  _USE_WRITE
  USERH_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USERH_ioctl,
#endif /* _USE_IOCTL == 1 */
};

static DSTATUS USERH_update_status(void)
{
  if ((Appli_state == APPLICATION_READY) &&
      (USBH_MSC_IsReady(&hUsbHostHS) == 1U) &&
      (USBH_MSC_UnitIsReady(&hUsbHostHS, USERH_MSC_LUN) == 1U))
  {
    Stat = 0U;
  }
  else
  {
    Stat = STA_NOINIT;
  }

  return Stat;
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USERH_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    (void)pdrv;
    return USERH_update_status();
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USERH_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    (void)pdrv;
    return USERH_update_status();
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USERH_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    (void)pdrv;

    if ((buff == NULL) || (count == 0U))
    {
      return RES_PARERR;
    }

    if ((USERH_update_status() & STA_NOINIT) != 0U)
    {
      return RES_NOTRDY;
    }

    if (USBH_MSC_Read(&hUsbHostHS, USERH_MSC_LUN, sector, buff, count) == USBH_OK)
    {
      return RES_OK;
    }

    return RES_ERROR;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USERH_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
    (void)pdrv;

    if ((buff == NULL) || (count == 0U))
    {
      return RES_PARERR;
    }

    if ((USERH_update_status() & STA_NOINIT) != 0U)
    {
      return RES_NOTRDY;
    }

    if (USBH_MSC_Write(&hUsbHostHS, USERH_MSC_LUN, sector, (uint8_t *)buff, count) == USBH_OK)
    {
      return RES_OK;
    }

    return RES_ERROR;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USERH_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    MSC_LUNTypeDef info;
    DRESULT res = RES_ERROR;

    (void)pdrv;

    if ((cmd != CTRL_SYNC) && (buff == NULL))
    {
      return RES_PARERR;
    }

    if ((USERH_update_status() & STA_NOINIT) != 0U)
    {
      return RES_NOTRDY;
    }

    switch (cmd)
    {
      case CTRL_SYNC:
        res = RES_OK;
        break;

      case GET_SECTOR_COUNT:
        if (USBH_MSC_GetLUNInfo(&hUsbHostHS, USERH_MSC_LUN, &info) == USBH_OK)
        {
          *(DWORD *)buff = info.capacity.block_nbr;
          res = RES_OK;
        }
        break;

      case GET_SECTOR_SIZE:
        if (USBH_MSC_GetLUNInfo(&hUsbHostHS, USERH_MSC_LUN, &info) == USBH_OK)
        {
          *(WORD *)buff = info.capacity.block_size;
          res = RES_OK;
        }
        break;

      case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1U;
        res = RES_OK;
        break;

      default:
        res = RES_PARERR;
        break;
    }

    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

