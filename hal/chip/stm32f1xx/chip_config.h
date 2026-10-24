/**
 * chip_config.h  —  STM32F103 芯片适配层：枚举 + HAL pin 宏
 *
 * 本文件是整个项目中**唯一**需要 include vendor HAL 头的配置文件。
 * HAL pin 宏 (GPIO_PIN_x, GPIOA, TIMx, ADCx 等) 全部集中在此，
 * 换芯片时只需改本文件和 hal_port_impl.c。
 */

#ifndef CHIP_CONFIG_STM32F1XX_H
#define CHIP_CONFIG_STM32F1XX_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* =====================================================================
 * ISA 枚举 — driver/app 层用这些逻辑名，不关心底层外设编号
 * ===================================================================== */

/* UART 逻辑标识 */
typedef enum {
    HAL_UART_DEBUG   = 0,
    HAL_UART_CONTROL = 1,
    HAL_UART_SIDE    = 2,
    HAL_UART_COUNT   = 3
} hal_uart_t;

/* GPIO 逻辑标识 */
typedef enum {
    HAL_PIN_BUTTON    = 0,
    HAL_PIN_POWER_EN  = 1,
    HAL_PIN_CHARGER   = 2,
    HAL_PIN_PPM       = 3,
    HAL_PIN_BUTTON1   = 4,
    HAL_PIN_BUTTON2   = 5,
    HAL_PIN_BUZZER    = 6,
    HAL_PIN_LED       = 7,
    HAL_PIN_PWM_CH1   = 8,
    HAL_PIN_PWM_CH2   = 9,
    HAL_PIN_COUNT
} hal_pin_t;

/* I2C 逻辑标识 */
typedef enum {
    HAL_I2C_NUNCHUK = 0,
    HAL_I2C_COUNT
} hal_i2c_t;

/* =====================================================================
 * FLASH 几何
 * ===================================================================== */
#define FLASH_BASE_ADDR       0x08000000UL
#define FLASH_PAGE_SIZE       2048UL
#define FLASH_PAGE_COUNT      128UL

/* =====================================================================
 * HAL PIN MACROS
 *
 * 下列宏原属 app/config/defines.h，已迁移到芯片适配层。
 * driver/app 层代码仍可用宏名，但值由此文件提供。
 * 换芯片时只需改本文件。
 * ===================================================================== */

/* 左电机霍尔 (PB5/PB6/PB7) */
#define LEFT_HALL_U_PIN GPIO_PIN_5
#define LEFT_HALL_V_PIN GPIO_PIN_6
#define LEFT_HALL_W_PIN GPIO_PIN_7
#define LEFT_HALL_U_PORT GPIOB
#define LEFT_HALL_V_PORT GPIOB
#define LEFT_HALL_W_PORT GPIOB

/* 右电机霍尔 (PC10/PC11/PC12) */
#define RIGHT_HALL_U_PIN GPIO_PIN_10
#define RIGHT_HALL_V_PIN GPIO_PIN_11
#define RIGHT_HALL_W_PIN GPIO_PIN_12
#define RIGHT_HALL_U_PORT GPIOC
#define RIGHT_HALL_V_PORT GPIOC
#define RIGHT_HALL_W_PORT GPIOC

/* 左电机 PWM (TIM8) */
#define LEFT_TIM TIM8
#define LEFT_TIM_U CCR1
#define LEFT_TIM_UH_PIN GPIO_PIN_6
#define LEFT_TIM_UH_PORT GPIOC
#define LEFT_TIM_UL_PIN GPIO_PIN_7
#define LEFT_TIM_UL_PORT GPIOA
#define LEFT_TIM_V CCR2
#define LEFT_TIM_VH_PIN GPIO_PIN_7
#define LEFT_TIM_VH_PORT GPIOC
#define LEFT_TIM_VL_PIN GPIO_PIN_0
#define LEFT_TIM_VL_PORT GPIOB
#define LEFT_TIM_W CCR3
#define LEFT_TIM_WH_PIN GPIO_PIN_8
#define LEFT_TIM_WH_PORT GPIOC
#define LEFT_TIM_WL_PIN GPIO_PIN_1
#define LEFT_TIM_WL_PORT GPIOB

/* 右电机 PWM (TIM1) */
#define RIGHT_TIM TIM1
#define RIGHT_TIM_U CCR1
#define RIGHT_TIM_UH_PIN GPIO_PIN_8
#define RIGHT_TIM_UH_PORT GPIOA
#define RIGHT_TIM_UL_PIN GPIO_PIN_13
#define RIGHT_TIM_UL_PORT GPIOB
#define RIGHT_TIM_V CCR2
#define RIGHT_TIM_VH_PIN GPIO_PIN_9
#define RIGHT_TIM_VH_PORT GPIOA
#define RIGHT_TIM_VL_PIN GPIO_PIN_14
#define RIGHT_TIM_VL_PORT GPIOB
#define RIGHT_TIM_W CCR3
#define RIGHT_TIM_WH_PIN GPIO_PIN_10
#define RIGHT_TIM_WH_PORT GPIOA
#define RIGHT_TIM_WL_PIN GPIO_PIN_15
#define RIGHT_TIM_WL_PORT GPIOB

/* 电流采样 ADC 引脚 */
#define LEFT_DC_CUR_PIN GPIO_PIN_0
#define LEFT_U_CUR_PIN GPIO_PIN_0
#define LEFT_V_CUR_PIN GPIO_PIN_3
#define LEFT_DC_CUR_PORT GPIOC
#define LEFT_U_CUR_PORT GPIOA
#define LEFT_V_CUR_PORT GPIOC

#define RIGHT_DC_CUR_PIN GPIO_PIN_1
#define RIGHT_U_CUR_PIN GPIO_PIN_4
#define RIGHT_V_CUR_PIN GPIO_PIN_5
#define RIGHT_DC_CUR_PORT GPIOC
#define RIGHT_U_CUR_PORT GPIOC
#define RIGHT_V_CUR_PORT GPIOC

/* DC-Link / Buzzer / LED / Power / Button / Charger */
#if BOARD_VARIANT == 0
  #define DCLINK_PIN  GPIO_PIN_2
  #define DCLINK_PORT GPIOC
  #define BUZZER_PIN  GPIO_PIN_4
  #define BUZZER_PORT GPIOA
  #define POWER_EN_PIN  GPIO_PIN_5
  #define POWER_EN_PORT GPIOA
  #define BUTTON_PIN  GPIO_PIN_1
  #define BUTTON_PORT GPIOA
  #define CHARGER_PIN  GPIO_PIN_12
  #define CHARGER_PORT GPIOA
#elif BOARD_VARIANT == 1
  #define DCLINK_PIN  GPIO_PIN_1
  #define DCLINK_PORT GPIOA
  #define BUZZER_PIN  GPIO_PIN_13
  #define BUZZER_PORT GPIOC
  #define POWER_EN_PIN  GPIO_PIN_15
  #define POWER_EN_PORT GPIOC
  #define BUTTON_PIN  GPIO_PIN_9
  #define BUTTON_PORT GPIOB
  #define CHARGER_PIN  GPIO_PIN_11
  #define CHARGER_PORT GPIOA
#endif

#define LED_PIN GPIO_PIN_2
#define LED_PORT GPIOB

/* 控制输入 */
#if defined(CONTROL_PPM_LEFT)
  #define PPM_PIN             GPIO_PIN_3
  #define PPM_PORT            GPIOA
#elif defined(CONTROL_PPM_RIGHT)
  #define PPM_PIN             GPIO_PIN_11
  #define PPM_PORT            GPIOB
#endif

#if defined(CONTROL_PWM_LEFT)
  #define PWM_PIN_CH1         GPIO_PIN_2
  #define PWM_PORT_CH1        GPIOA
  #define PWM_PIN_CH2         GPIO_PIN_3
  #define PWM_PORT_CH2        GPIOA
#elif defined(CONTROL_PWM_RIGHT)
  #define PWM_PIN_CH1         GPIO_PIN_10
  #define PWM_PORT_CH1        GPIOB
  #define PWM_PIN_CH2         GPIO_PIN_11
  #define PWM_PORT_CH2        GPIOB
#endif

#if defined(SUPPORT_BUTTONS_LEFT)
  #define BUTTON1_PIN         GPIO_PIN_2
  #define BUTTON1_PORT        GPIOA
  #define BUTTON2_PIN         GPIO_PIN_3
  #define BUTTON2_PORT        GPIOA
#elif defined(SUPPORT_BUTTONS_RIGHT)
  #define BUTTON1_PIN         GPIO_PIN_10
  #define BUTTON1_PORT        GPIOB
  #define BUTTON2_PIN         GPIO_PIN_11
  #define BUTTON2_PORT        GPIOB
#endif

/* 占位注释 */
#define CHIP_STM32F1XX

#endif /* CHIP_CONFIG_STM32F1XX_H */
