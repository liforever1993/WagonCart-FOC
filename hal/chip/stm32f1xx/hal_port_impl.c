/**
 * hal_port_impl.c  —  STM32F103 芯片适配层 ISA 实现
 *
 * 这是**唯一**直接 include STM32 HAL 的应用代码文件。
 * 换芯片时只需：复制整个 chip/stm32f1xx/ 目录为 chip/<新芯片>/，
 * 改 chip_config.h 的枚举值和本文件的实现。
 *
 * ISA 接口定义见 hal/chip/hal_port.h
 */

#include "hal_port.h"           /* 可移植接口 */
#include "stm32f1xx_hal.h"      /* ← 本文件是唯一允许 include vendor HAL 的 driver/app 文件 */
#include "defines.h"            /* 保留 — 含软件常量 */

/* =====================================================================
 * 外部声明 — 由 chip_init.c 在启动时创建的 HAL 句柄
 * ===================================================================== */
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern I2C_HandleTypeDef hi2c2;

/* =====================================================================
 * 内部辅助：hal_uart_t → HAL 句柄指针
 * ===================================================================== */
static inline UART_HandleTypeDef *uart_handle(hal_uart_t uart) {
    /*
     * 每个逻辑 UART 独立映射到一个物理 USART。
     * 编译时宏选择哪个 USART 实例：
     *   DEBUG  → DEBUG_SERIAL_USART2/3
     *   CONTROL → CONTROL_SERIAL_USART2/3
     *   SIDE   → SIDEBOARD_SERIAL_USART2/3 或 FEEDBACK_SERIAL_USART2
     *
     * 两个 USART 可以同时存在（FEEDBACK_SERIAL_USART2 + FEEDBACK_SERIAL_USART3）。
     */
    switch (uart) {
#if defined(DEBUG_SERIAL_USART2)
        case HAL_UART_DEBUG: return &huart2;
#elif defined(DEBUG_SERIAL_USART3)
        case HAL_UART_DEBUG: return &huart3;
#endif

#if defined(CONTROL_SERIAL_USART2)
        case HAL_UART_CONTROL: return &huart2;
#elif defined(CONTROL_SERIAL_USART3)
        case HAL_UART_CONTROL: return &huart3;
#elif defined(FEEDBACK_SERIAL_USART3)
        case HAL_UART_CONTROL: return &huart3;
#endif

#if defined(SIDEBOARD_SERIAL_USART2) || defined(FEEDBACK_SERIAL_USART2)
        case HAL_UART_SIDE: return &huart2;
#elif defined(SIDEBOARD_SERIAL_USART3)
        case HAL_UART_SIDE: return &huart3;
#endif
        default: return NULL;
    }
}

/* =====================================================================
 * 时间
 * ===================================================================== */
void hal_delay_ms(uint32_t ms)  { HAL_Delay(ms); }
void hal_delay_us(uint32_t us)  { /* SysTick us 级延时；如无实现则 busy-wait */
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000U) * us;
    while ((DWT->CYCCNT - start) < ticks);
}
uint32_t hal_get_tick_ms(void)  { return HAL_GetTick(); }

/* =====================================================================
 * UART
 * ===================================================================== */
void hal_tty_write(hal_uart_t uart, const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    UART_HandleTypeDef *h = uart_handle(uart);
    if (h) HAL_UART_Transmit(h, (uint8_t *)data, len, timeout_ms);
}

void hal_tty_start_rx_dma(hal_uart_t uart, uint8_t *buf, uint16_t len) {
    /* STM32F1 HAL 无 HAL_UARTEx_ReceiveToIdle_DMA — 用 HAL_UART_Receive_DMA
     * + IDLE 中断通知 frame 结束（IDLE 检测在 chip_irq.c 中处理）*/
    UART_HandleTypeDef *h = uart_handle(uart);
    if (h) {
        HAL_UART_Receive_DMA(h, buf, len);
        __HAL_UART_ENABLE_IT(h, UART_IT_IDLE);
        /* DMA circular mode 在 chip_init.c 配置 */
    }
}

uint8_t hal_tty_rx_idle_flag(hal_uart_t uart) {
    UART_HandleTypeDef *h = uart_handle(uart);
    if (!h) return 0;
    if (__HAL_UART_GET_FLAG(h, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(h);
        return 1;
    }
    return 0;
}

void hal_tty_disable_rx_errors(hal_uart_t uart) {
    /* 原 util.c 的 UART_DisableRxErrors() 直接写寄存器 CR1/CR3 */
    UART_HandleTypeDef *h = uart_handle(uart);
    if (!h) return;
    CLEAR_BIT(h->Instance->CR1, USART_CR1_PEIE);
    CLEAR_BIT(h->Instance->CR3, USART_CR3_EIE);
    /* NF/EI 不关闭，DMA 停止条件需要它们 */
}

uint16_t hal_tty_dma_pos(hal_uart_t uart, uint16_t buf_len) {
    /* 原 util.c 的 __HAL_DMA_GET_COUNTER(huart.hdmarx) 包装 */
    UART_HandleTypeDef *h = uart_handle(uart);
    if (!h || !h->hdmarx) return 0;
    uint16_t remaining = (uint16_t)__HAL_DMA_GET_COUNTER(h->hdmarx);
    return (uint16_t)(buf_len - remaining);
}

void hal_tty_write_dma(hal_uart_t uart, uint8_t *data, uint16_t len) {
    UART_HandleTypeDef *h = uart_handle(uart);
    if (h) HAL_UART_Transmit_DMA(h, data, len);
}

uint8_t hal_tty_dma_tx_busy(hal_uart_t uart) {
    UART_HandleTypeDef *h = uart_handle(uart);
    if (!h || !h->hdmatx) return 0;
    return (__HAL_DMA_GET_COUNTER(h->hdmatx) != 0) ? 1 : 0;
}

/* =====================================================================
 * FLASH — 对应 driver/eeprom.c 的 HAL_FLASH* 调用
 * ===================================================================== */
uint16_t hal_flash_page_size(void) { return FLASH_PAGE_SIZE; }

hal_status_t hal_flash_erase_page(uint32_t page_base_addr) {
    FLASH_EraseInitTypeDef s = {0};
    uint32_t page_error = 0;
    s.TypeErase    = FLASH_TYPEERASE_PAGES;
    s.PageAddress  = page_base_addr;
    s.NbPages      = 1;
    return (hal_status_t)HAL_FLASHEx_Erase(&s, &page_error);
}

hal_status_t hal_flash_program_halfword(uint32_t addr, uint16_t data) {
    return (hal_status_t)HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, data);
}

hal_status_t hal_flash_program_word(uint32_t addr, uint32_t data) {
    return (hal_status_t)HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
}

void hal_flash_unlock(void) { HAL_FLASH_Unlock(); }
void hal_flash_lock(void)  { HAL_FLASH_Lock(); }

/* =====================================================================
 * GPIO — pin_map 根据 BOARD_VARIANT 选择正确的 PORT/PIN
 * ===================================================================== */
struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} static const pin_map[] = {
#if BOARD_VARIANT == 0
    [HAL_PIN_BUTTON]   = { GPIOA, GPIO_PIN_1  },
    [HAL_PIN_POWER_EN] = { GPIOA, GPIO_PIN_5  },
    [HAL_PIN_CHARGER]  = { GPIOA, GPIO_PIN_12 },
    [HAL_PIN_BUZZER]   = { GPIOA, GPIO_PIN_4  },
#elif BOARD_VARIANT == 1
    [HAL_PIN_BUTTON]   = { GPIOB, GPIO_PIN_9  },
    [HAL_PIN_POWER_EN] = { GPIOC, GPIO_PIN_15 },
    [HAL_PIN_CHARGER]  = { GPIOA, GPIO_PIN_11 },
    [HAL_PIN_BUZZER]   = { GPIOC, GPIO_PIN_13 },
#endif
    [HAL_PIN_LED]       = { GPIOB, GPIO_PIN_2  },
#if defined(CONTROL_PPM_LEFT)
    [HAL_PIN_PPM]      = { GPIOA, GPIO_PIN_3  },
#elif defined(CONTROL_PPM_RIGHT)
    [HAL_PIN_PPM]      = { GPIOB, GPIO_PIN_11 },
#endif
#if defined(SUPPORT_BUTTONS_LEFT)
    [HAL_PIN_BUTTON1]  = { GPIOA, GPIO_PIN_2  },
    [HAL_PIN_BUTTON2]  = { GPIOA, GPIO_PIN_3  },
#elif defined(SUPPORT_BUTTONS_RIGHT)
    [HAL_PIN_BUTTON1]  = { GPIOB, GPIO_PIN_10 },
    [HAL_PIN_BUTTON2]  = { GPIOB, GPIO_PIN_11 },
#endif
#if defined(CONTROL_PWM_LEFT)
    [HAL_PIN_PWM_CH1]  = { GPIOA, GPIO_PIN_2  },
    [HAL_PIN_PWM_CH2]  = { GPIOA, GPIO_PIN_3  },
#elif defined(CONTROL_PWM_RIGHT)
    [HAL_PIN_PWM_CH1]  = { GPIOB, GPIO_PIN_10 },
    [HAL_PIN_PWM_CH2]  = { GPIOB, GPIO_PIN_11 },
#endif
};

uint8_t hal_gpio_read(hal_pin_t pin) {
    if (pin >= sizeof(pin_map)/sizeof(pin_map[0])) return 0;
    return HAL_GPIO_ReadPin(pin_map[pin].port, pin_map[pin].pin);
}

void hal_gpio_write(hal_pin_t pin, uint8_t level) {
    if (pin >= sizeof(pin_map)/sizeof(pin_map[0])) return;
    HAL_GPIO_WritePin(pin_map[pin].port, pin_map[pin].pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* =====================================================================
 * I2C — Nunchuk 等 I2C 外设访问
 * ===================================================================== */
static inline I2C_HandleTypeDef *i2c_handle(hal_i2c_t i2c) {
    switch (i2c) {
        case HAL_I2C_NUNCHUK: return &hi2c2;
        default: return NULL;
    }
}

hal_status_t hal_i2c_master_transmit(hal_i2c_t i2c, uint8_t addr,
                                     uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    I2C_HandleTypeDef *h = i2c_handle(i2c);
    if (!h) return HAL_PORT_ERROR;
    return (hal_status_t)HAL_I2C_Master_Transmit(h, addr, data, len, timeout_ms);
}

hal_status_t hal_i2c_master_receive(hal_i2c_t i2c, uint8_t addr,
                                    uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    I2C_HandleTypeDef *h = i2c_handle(i2c);
    if (!h) return HAL_PORT_ERROR;
    return (hal_status_t)HAL_I2C_Master_Receive(h, addr, data, len, timeout_ms);
}

/* =====================================================================
 * CHIP 信息
 * ===================================================================== */
const char *hal_chip_name(void) { return "STM32F103RC"; }
