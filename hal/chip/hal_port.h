/**
 * hal_port.h  —  可移植硬件抽象接口（ISA）
 *
 * 所有 app/ 与 driver/ 层代码 ONLY include 这一个头文件即可，
 * 不直接 include 任何 vendor HAL / CMSIS 头文件。
 *
 * 换芯片只需：
 *   1. 新建 chip/<新芯片>/ 目录（实现本文件声明的所有 hal_* 函数）
 *   2. Makefile 里 CHIP=<新芯片> 即可
 *
 * 本文件不依赖任何 vendor 代码，只依赖 C 标准库头。
 */

#ifndef HAL_PORT_H
#define HAL_PORT_H

#include <stdint.h>
#include <stddef.h>

/* =====================================================================
 * 编译时选择 chip_config.h
 * Makefile 通过 -DCHIP_STM32F1XX 等方式注入 CHIP_<VENDOR> 宏，
 * 或通过 -I chip/<dir> 让 chip_switch.h 隐式找到对应 chip_config.h
 * ===================================================================== */
#if defined(CHIP_STM32F1XX) || defined(CHIP_STM32F1)
  #include "chip_config.h"
#else
  /* 默认 fallback — Makefile 必须正确传入 -DCHIP_xxx */
  #include "chip_config.h"
#endif

/* =====================================================================
 * 通用状态码（与 vendor HAL 解耦）
 * ===================================================================== */
typedef enum {
    HAL_PORT_OK      = 0,
    HAL_PORT_ERROR   = 1,
    HAL_PORT_BUSY    = 2,
    HAL_PORT_TIMEOUT = 3
} hal_status_t;

/* =====================================================================
 * TIME / DELAY
 * ===================================================================== */
void     hal_delay_ms(uint32_t ms);
void     hal_delay_us(uint32_t us);
uint32_t hal_get_tick_ms(void);

/* =====================================================================
 * UART / TTY
 *
 * uart 参数来自 chip_config.h 中定义的 hal_uart_t 枚举，
 * chip 层实现负责把它映射到底层外设实例（STM32 的 huart 指针、
 * ESP32 的 UART port 编号、etc.）。
 * ===================================================================== */
void hal_tty_write(hal_uart_t uart, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
void hal_tty_start_rx_dma(hal_uart_t uart, uint8_t *buf, uint16_t len);
uint8_t hal_tty_rx_idle_flag(hal_uart_t uart);
void hal_tty_disable_rx_errors(hal_uart_t uart);

/* 环形 DMA 缓冲区当前"已填充"位置（0 ~ buf_len） */
uint16_t hal_tty_dma_pos(hal_uart_t uart, uint16_t buf_len);

/* DMA TX 发送：启动 DMA 异步发送；返回 1 = 仍有 DMA 发送在进行中 */
void hal_tty_write_dma(hal_uart_t uart, uint8_t *data, uint16_t len);
uint8_t hal_tty_dma_tx_busy(hal_uart_t uart);

/* =====================================================================
 * I2C
 * ===================================================================== */
hal_status_t hal_i2c_master_transmit(hal_i2c_t i2c, uint8_t addr,
                                     uint8_t *data, uint16_t len, uint32_t timeout_ms);
hal_status_t hal_i2c_master_receive(hal_i2c_t i2c, uint8_t addr,
                                    uint8_t *data, uint16_t len, uint32_t timeout_ms);

/* =====================================================================
 * FLASH / EEPROM 仿真
 *
 * 为了让 driver/eeprom.c 在不同 flash 组织（页面大小/擦除粒度）
 * 上都能工作，chip 层需要提供以下 API。
 * ===================================================================== */
/* 返回最小擦除单元大小（字节）—— STM32F103 为 2KB page */
uint16_t hal_flash_page_size(void);

/* 擦除包含给定地址的最小页 */
hal_status_t hal_flash_erase_page(uint32_t page_base_addr);

/* 编程一个半字（16-bit）。driver 负责对齐。 */
hal_status_t hal_flash_program_halfword(uint32_t addr, uint16_t data);

/* 编程一个完整 word（32-bit）。driver 负责对齐。 */
hal_status_t hal_flash_program_word(uint32_t addr, uint32_t data);

/* 内部 flash 写保护的解锁/锁定 */
void hal_flash_unlock(void);
void hal_flash_lock(void);

/* =====================================================================
 * GPIO（按键、霍尔、编码器输入等）
 * ===================================================================== */
uint8_t hal_gpio_read(hal_pin_t pin);

/* 设置 GPIO 输出引脚电平。level: 0 = LOW, 非 0 = HIGH */
void hal_gpio_write(hal_pin_t pin, uint8_t level);

/* =====================================================================
 * CHIP 信息
 * ===================================================================== */
const char *hal_chip_name(void);

#endif /* HAL_PORT_H */
