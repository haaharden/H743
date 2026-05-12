/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    userh_diskio.h
  * @brief   This file contains the common defines and functions prototypes for
  *          the userh_diskio driver.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USERH_DISKIO_H
#define __USERH_DISKIO_H

#ifdef __cplusplus
 extern "C" {
#endif

/* USER CODE BEGIN 0 */

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
extern Diskio_drvTypeDef  USERH_Driver;

/* USER CODE END 0 */

#ifdef __cplusplus
}
#endif

#endif /* __USERH_DISKIO_H */
