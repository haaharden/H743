#include "touch_gt1151q.h"

#include <string.h>

// 这版实现按旧驱动的真实工作路径恢复：专用 I2C2 句柄、较慢时序、专用复位和取点流程。

#define GTP_I2C                        I2C2
#define GTP_I2C_ADDRESS                TOUCH_I2C_ADDRESS
#define GTP_I2C_TIMEOUT_MS             1000U
#define GTP_I2C_RECOVERY_DELAY_US      5U
#define GTP_I2C_RECOVERY_PULSE_COUNT   9U

#define GTP_I2C_SCL_PIN                GPIO_PIN_4
#define GTP_I2C_SCL_GPIO_PORT          GPIOH
#define GTP_I2C_SCL_AF                 GPIO_AF4_I2C2

#define GTP_I2C_SDA_PIN                GPIO_PIN_5
#define GTP_I2C_SDA_GPIO_PORT          GPIOH
#define GTP_I2C_SDA_AF                 GPIO_AF4_I2C2

#define GTP_REG_COMMAND                0x8040U
#define GTP_REG_CONFIG_DATA            0x8050U
#define GTP_REG_VERSION                0x8140U
#define GTP_READ_COOR_ADDR             0x814EU

#define GTP_ADDR_LENGTH                2U
#define GTP_MAX_TOUCH                  5U
#define GTP_POINT_FRAME_SIZE           12U
#define GTP_BUFFER_STATUS_READY        0x80U
#define GTP_TOUCH_COUNT_MASK           0x0FU

#define I2C_M_RD                       0x0001U

typedef struct {
  uint8_t addr;
  uint16_t flags;
  uint16_t len;
  uint8_t *buf;
} touch_i2c_msg_t;

static I2C_HandleTypeDef s_touch_i2c_handle;
static volatile uint8_t s_touch_event_pending = 0U;

static void Touch_DelayUs(uint32_t us);
static void Touch_I2cGpioConfig(void);
static void Touch_I2cGpioConfigAlternate(void);
static void Touch_I2cGpioConfigOpenDrain(void);
static void Touch_I2cModeConfig(void);
static void Touch_BusRecovery(void);
static uint32_t Touch_RecoverFromError(void);
static int32_t Touch_I2cTransfer(touch_i2c_msg_t *msgs, int32_t num);
static int32_t GTP_I2C_Read(uint8_t client_addr, uint8_t *buf, int32_t len);
static int32_t GTP_I2C_Write(uint8_t client_addr, uint8_t *buf, int32_t len);
static int8_t GTP_I2C_Test(void);

static void Touch_DelayUs(uint32_t us)
{
  uint32_t cycles_per_loop;
  volatile uint32_t count;

  cycles_per_loop = HAL_RCC_GetHCLKFreq() / 3000000U;
  if (cycles_per_loop == 0U)
  {
    cycles_per_loop = 1U;
  }

  while (us-- > 0U)
  {
    for (count = cycles_per_loop; count > 0U; count--)
    {
      __NOP();
    }
  }
}

static void Touch_I2cGpioConfigAlternate(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = GTP_I2C_SCL_PIN;
  gpio_init.Mode = GPIO_MODE_AF_OD;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Alternate = GTP_I2C_SCL_AF;
  HAL_GPIO_Init(GTP_I2C_SCL_GPIO_PORT, &gpio_init);

  gpio_init.Pin = GTP_I2C_SDA_PIN;
  gpio_init.Alternate = GTP_I2C_SDA_AF;
  HAL_GPIO_Init(GTP_I2C_SDA_GPIO_PORT, &gpio_init);
}

static void Touch_I2cGpioConfigOpenDrain(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = GTP_I2C_SCL_PIN;
  gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GTP_I2C_SCL_GPIO_PORT, &gpio_init);

  gpio_init.Pin = GTP_I2C_SDA_PIN;
  HAL_GPIO_Init(GTP_I2C_SDA_GPIO_PORT, &gpio_init);

  HAL_GPIO_WritePin(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN, GPIO_PIN_SET);
}

static void Touch_I2cGpioConfig(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_I2C2_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  Touch_I2cGpioConfigAlternate();

  gpio_init.Pin = CTP_RST_Pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(CTP_RST_GPIO_Port, &gpio_init);

  gpio_init.Pin = CTP_INT_Pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio_init);
}

static void Touch_I2cModeConfig(void)
{
  // 旧驱动这里故意把触摸 I2C 压到更稳的时序，不能直接用 CubeMX 那套更激进的 hi2c2 配置替代。
  s_touch_i2c_handle.Instance = GTP_I2C;
  s_touch_i2c_handle.Init.Timing = 0x90913232U;
  s_touch_i2c_handle.Init.OwnAddress1 = 0U;
  s_touch_i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  s_touch_i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  s_touch_i2c_handle.Init.OwnAddress2 = 0U;
  s_touch_i2c_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  s_touch_i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  s_touch_i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  HAL_I2C_Init(&s_touch_i2c_handle);
  HAL_I2CEx_AnalogFilter_Config(&s_touch_i2c_handle, I2C_ANALOGFILTER_ENABLE);
}

static void Touch_BusRecovery(void)
{
  uint32_t pulse;

  if (s_touch_i2c_handle.Instance != NULL)
  {
    HAL_I2C_DeInit(&s_touch_i2c_handle);
  }

  Touch_I2cGpioConfigOpenDrain();
  Touch_DelayUs(GTP_I2C_RECOVERY_DELAY_US);

  HAL_GPIO_WritePin(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN, GPIO_PIN_SET);
  for (pulse = 0U; pulse < GTP_I2C_RECOVERY_PULSE_COUNT; pulse++)
  {
    HAL_GPIO_WritePin(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN, GPIO_PIN_SET);
    Touch_DelayUs(GTP_I2C_RECOVERY_DELAY_US);
    HAL_GPIO_WritePin(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN, GPIO_PIN_RESET);
    Touch_DelayUs(GTP_I2C_RECOVERY_DELAY_US);
  }

  HAL_GPIO_WritePin(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN, GPIO_PIN_RESET);
  Touch_DelayUs(GTP_I2C_RECOVERY_DELAY_US);
  HAL_GPIO_WritePin(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN, GPIO_PIN_SET);
  Touch_DelayUs(GTP_I2C_RECOVERY_DELAY_US);
  HAL_GPIO_WritePin(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN, GPIO_PIN_SET);
  Touch_DelayUs(GTP_I2C_RECOVERY_DELAY_US);

  Touch_I2cGpioConfigAlternate();
}

static uint32_t Touch_RecoverFromError(void)
{
  Touch_BusRecovery();
  Touch_I2cModeConfig();
  return 1U;
}

static int32_t Touch_I2cTransfer(touch_i2c_msg_t *msgs, int32_t num)
{
  int32_t index;
  int32_t result = 0;

  for (index = 0; (index < num) && (result == 0); index++)
  {
    if ((msgs[index].flags & I2C_M_RD) != 0U)
    {
      result = (int32_t)I2C_ReadBytes(msgs[index].addr, msgs[index].buf, msgs[index].len);
    }
    else
    {
      result = (int32_t)I2C_WriteBytes(msgs[index].addr, msgs[index].buf, (uint8_t)msgs[index].len);
    }
  }

  if (result != 0)
  {
    return result;
  }

  return index;
}

static int32_t GTP_I2C_Read(uint8_t client_addr, uint8_t *buf, int32_t len)
{
  touch_i2c_msg_t msgs[2];
  int32_t ret = -1;
  int32_t retries = 0;

  msgs[0].flags = 0U;
  msgs[0].addr = client_addr;
  msgs[0].len = GTP_ADDR_LENGTH;
  msgs[0].buf = &buf[0];

  msgs[1].flags = I2C_M_RD;
  msgs[1].addr = client_addr;
  msgs[1].len = (uint16_t)(len - (int32_t)GTP_ADDR_LENGTH);
  msgs[1].buf = &buf[GTP_ADDR_LENGTH];

  while (retries < 5)
  {
    ret = Touch_I2cTransfer(msgs, 2);
    if (ret == 2)
    {
      break;
    }
    retries++;
  }

  return ret;
}

static int32_t GTP_I2C_Write(uint8_t client_addr, uint8_t *buf, int32_t len)
{
  touch_i2c_msg_t msg;
  int32_t ret = -1;
  int32_t retries = 0;

  msg.flags = 0U;
  msg.addr = client_addr;
  msg.len = (uint16_t)len;
  msg.buf = buf;

  while (retries < 5)
  {
    ret = Touch_I2cTransfer(&msg, 1);
    if (ret == 1)
    {
      break;
    }
    retries++;
  }

  return ret;
}

static int8_t GTP_I2C_Test(void)
{
  uint8_t test[3] = {(uint8_t)(GTP_REG_CONFIG_DATA >> 8), (uint8_t)(GTP_REG_CONFIG_DATA & 0xFFU), 0U};
  uint8_t retry = 0U;
  int8_t ret = -1;

  while (retry++ < 5U)
  {
    ret = (int8_t)GTP_I2C_Read(GTP_I2C_ADDRESS, test, 3);
    if (ret > 0)
    {
      return ret;
    }
  }

  return ret;
}

HAL_StatusTypeDef Touch_Init(void)
{
  if (GTP_Init_Panel() == 0)
  {
    return HAL_OK;
  }

  return HAL_ERROR;
}

uint8_t Touch_ReadPoint(uint16_t *x, uint16_t *y)
{
  int touch_x;
  int touch_y;

  if ((x == NULL) || (y == NULL))
  {
    return 0U;
  }

  if (GTP_Execu(&touch_x, &touch_y) == 0)
  {
    return 0U;
  }

  *x = (uint16_t)touch_x;
  *y = (uint16_t)touch_y;
  return 1U;
}

uint8_t Touch_HasEventPending(void)
{
  return s_touch_event_pending;
}

void Touch_ClearEventPending(void)
{
  s_touch_event_pending = 0U;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == CTP_INT_Pin)
  {
    // 中断里只置位，真正的 I2C 访问仍然放在线程上下文做。
    s_touch_event_pending = 1U;
  }
}

void I2C_Touch_Init(void)
{
  Touch_I2cGpioConfig();
  s_touch_i2c_handle.Instance = GTP_I2C;

  // 上电先做一次总线恢复，避免异常复位后 BUSY 卡死。
  Touch_BusRecovery();
  Touch_I2cModeConfig();

  I2C_ResetChip();
  GTP_IRQ_Disable();
}

uint32_t I2C_WriteBytes(uint8_t clientAddr, uint8_t *pBuffer, uint8_t numByteToWrite)
{
  if (HAL_I2C_Master_Transmit(&s_touch_i2c_handle, clientAddr, pBuffer, numByteToWrite, GTP_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0U;
  }

  return Touch_RecoverFromError();
}

uint32_t I2C_ReadBytes(uint8_t clientAddr, uint8_t *pBuffer, uint16_t numByteToRead)
{
  if (HAL_I2C_Master_Receive(&s_touch_i2c_handle, clientAddr, pBuffer, numByteToRead, GTP_I2C_TIMEOUT_MS) == HAL_OK)
  {
    return 0U;
  }

  return Touch_RecoverFromError();
}

void I2C_ResetChip(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = CTP_INT_Pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio_init);

  // GT1151QM 的地址选择窗口里，把 INT 维持为低电平，保持和旧驱动一致。
  HAL_GPIO_WritePin(CTP_INT_GPIO_Port, CTP_INT_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_RESET);
  Touch_DelayUs(10U);

  HAL_GPIO_WritePin(CTP_RST_GPIO_Port, CTP_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(20U);

  gpio_init.Pin = CTP_INT_Pin;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio_init);
}

void I2C_GTP_IRQDisable(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = CTP_INT_Pin;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio_init);

  HAL_NVIC_SetPriority(CTP_INT_EXTI_IRQn, 5, 0);
  HAL_NVIC_DisableIRQ(CTP_INT_EXTI_IRQn);
}

void I2C_GTP_IRQEnable(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = CTP_INT_Pin;
  gpio_init.Mode = GPIO_MODE_IT_RISING_FALLING;
  gpio_init.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(CTP_INT_GPIO_Port, &gpio_init);

  HAL_NVIC_SetPriority(CTP_INT_EXTI_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(CTP_INT_EXTI_IRQn);
}

int8_t GTP_Reset_Guitar(void)
{
  I2C_ResetChip();
  return 0;
}

int32_t GTP_Read_Version(void)
{
  int32_t ret;
  uint8_t buf[8] = {(uint8_t)(GTP_REG_VERSION >> 8), (uint8_t)(GTP_REG_VERSION & 0xFFU)};

  ret = GTP_I2C_Read(GTP_I2C_ADDRESS, buf, (int32_t)sizeof(buf));
  if (ret < 0)
  {
    return ret;
  }

  return 0;
}

void GTP_IRQ_Disable(void)
{
  I2C_GTP_IRQDisable();
}

void GTP_IRQ_Enable(void)
{
  I2C_GTP_IRQEnable();
}

int32_t GTP_Init_Panel(void)
{
  int32_t ret;

  I2C_Touch_Init();

  ret = GTP_I2C_Test();
  if (ret < 0)
  {
    return ret;
  }

  HAL_Delay(100U);
  (void)GTP_Read_Version();

  s_touch_event_pending = 0U;
  I2C_GTP_IRQEnable();
  return 0;
}

int8_t GTP_Send_Command(uint8_t command)
{
  int8_t ret = -1;
  int8_t retry = 0;
  uint8_t command_buf[3] = {(uint8_t)(GTP_REG_COMMAND >> 8), (uint8_t)(GTP_REG_COMMAND & 0xFFU), command};

  while (retry++ < 5)
  {
    ret = (int8_t)GTP_I2C_Write(GTP_I2C_ADDRESS, command_buf, 3);
    if (ret > 0)
    {
      return ret;
    }
  }

  return ret;
}

int GTP_Execu(int *x, int *y)
{
  uint8_t end_cmd[3] = {(uint8_t)(GTP_READ_COOR_ADDR >> 8), (uint8_t)(GTP_READ_COOR_ADDR & 0xFFU), 0U};
  uint8_t point_data[GTP_POINT_FRAME_SIZE] = {(uint8_t)(GTP_READ_COOR_ADDR >> 8), (uint8_t)(GTP_READ_COOR_ADDR & 0xFFU)};
  uint8_t finger;
  uint8_t touch_num;
  int32_t input_x;
  int32_t input_y;
  int32_t ret;

  if ((x == NULL) || (y == NULL))
  {
    return 0;
  }

  ret = GTP_I2C_Read(GTP_I2C_ADDRESS, point_data, (int32_t)sizeof(point_data));
  if (ret < 0)
  {
    return 0;
  }

  finger = point_data[GTP_ADDR_LENGTH];
  if (finger == 0x00U)
  {
    return 0;
  }

  if ((finger & GTP_BUFFER_STATUS_READY) == 0U)
  {
    goto exit_work_func;
  }

  touch_num = finger & GTP_TOUCH_COUNT_MASK;
  if (touch_num > GTP_MAX_TOUCH)
  {
    goto exit_work_func;
  }

  if (touch_num != 0U)
  {
    input_x = (int32_t)point_data[4] | ((int32_t)point_data[5] << 8);
    input_y = (int32_t)point_data[6] | ((int32_t)point_data[7] << 8);

    if ((input_x < (int32_t)TOUCH_MAX_WIDTH) && (input_y < (int32_t)TOUCH_MAX_HEIGHT))
    {
      *x = input_x;
      *y = input_y;
    }
    else
    {
      goto exit_work_func;
    }
  }

exit_work_func:
  ret = GTP_I2C_Write(GTP_I2C_ADDRESS, end_cmd, 3);
  if (ret < 0)
  {
    return 0;
  }

  return (int)touch_num;
}

uint8_t GTP_HasEventPending(void)
{
  return Touch_HasEventPending();
}

void GTP_ClearEventPending(void)
{
  Touch_ClearEventPending();
}