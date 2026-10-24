######################################
# target
######################################
TARGET = WagonCart

######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og

######################################
# chip selection
#
# CHIP = stm32f1xx   → STM32F103  (default)
# CHIP = stm32f4xx   → STM32F407
# CHIP = esp32        → ESP32
# CHIP = gd32f3xx     → GD32 (ST clone)
# CHIP = lpc1768      → NXP Cortex-M3
# CHIP = any_chip_x   → 只要在配置表里有一行
#
# 加新芯片的步骤：
#   1. mkdir hal/chip/<new>
#   2. 复制 vendor SDK 到 hal/chip/<new>/vendor/
#      (HAL driver / CMSIS device / 其他)
#   3. 写 hal/chip/<new>/hal_port_impl.c 实现 ISA
#   4. 在下方 CHIP 配置表追加一行
#   5. make CHIP=<new>
#
# 目录约定（每个 chip 完全自包含）：
#   hal/chip/<chip>/
#   ├── vendor/          ← 芯片专属 vendor SDK (HAL/CMSIS/...)
#   ├── chip_init.c/h
#   ├── chip_irq.c/h
#   ├── hal_port_impl.c    # 唯一直接 #include vendor HAL 的应用文件
#   ├── chip_config.h
#   ├── *.ld             ← 链接脚本
#   ├── startup*.s       ← 启动文件
#   └── ...
#
# ISA 接口（driver/app 唯一允许 include 的 HAL 相关头）：
#   hal/chip/hal_port.h
#
# 跨芯片共享的 Cortex-M 内核头（ARM 官方，与芯片无关）：
#   Drivers/CMSIS/Include/core_cm*.h
######################################
CHIP ?= stm32f1xx

# ─── 每个 chip 一个配置块 ────────────────────────────────────────────────
# 字段说明：
#   CHIP_CPU         编译器 CPU flag  (-mcpu=...)
#   CHIP_FPU         FPU flag 串     (-mfpu=... -mfloat-abi=... 或空)
#   CHIP_DEFS        编译宏         (-DUSE_HAL_DRIVER -DXXX ...)
#   CHIP_INCLUDES    vendor HAL/CMSIS 头路径 (相对 hal/chip/<chip>/ 的 -I 列表)
#   CHIP_HAL_SOURCES vendor HAL 源文件 (相对 hal/chip/<chip>/ 的列表)
#   CHIP_LD          链接脚本路径     (相对 hal/chip/<chip>/)
#   CHIP_ASM         启动文件路径     (相对 hal/chip/<chip>/)
#
# 通用 CMSIS Cortex-M 内核头（ARM 官方 core_cm*.h）固定从
# Drivers/CMSIS/Include/ 引用，所有 Cortex-M 芯片共享。
# ------------------------------------------------------------------------

ifeq ($(CHIP),stm32f1xx)
  CHIP_CPU   = -mcpu=cortex-m3
  CHIP_FPU   = -msoft-float
  CHIP_DEFS  = -DUSE_HAL_DRIVER -DSTM32F103xE -DCHIP_STM32F1XX
  CHIP_INCLUDES = \
    vendor/STM32F1xx_HAL_Driver/Inc \
    vendor/STM32F1xx_HAL_Driver/Inc/Legacy \
    vendor/CMSIS/Device/ST/STM32F1xx/Include
  CHIP_HAL_SOURCES = \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c \
    vendor/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
  CHIP_LD    = STM32F103RCTx_FLASH.ld
  CHIP_ASM   = startup_stm32f103xe.s
endif

ifeq ($(CHIP),stm32f4xx)
  # 说明：需先把 STM32F4xx HAL 拷贝到 hal/chip/stm32f4xx/vendor/
  #       以及 CMSIS/Device/ST/STM32F4xx/ 和 startup/linker script
  CHIP_CPU   = -mcpu=cortex-m4
  CHIP_FPU   = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  CHIP_DEFS  = -DUSE_HAL_DRIVER -DSTM32F407xx -DCHIP_STM32F4XX
  CHIP_INCLUDES = \
    vendor/STM32F4xx_HAL_Driver/Inc \
    vendor/STM32F4xx_HAL_Driver/Inc/Legacy \
    vendor/CMSIS/Device/ST/STM32F4xx/Include
  CHIP_HAL_SOURCES = \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio_ex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c \
    vendor/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
  CHIP_LD    = STM32F407ZGTx_FLASH.ld
  CHIP_ASM   = startup_stm32f407xx.s
endif

# ─── 非 Cortex-M 芯片示例（占位，添加时按需改）───────────────────────────
# ifeq ($(CHIP),esp32)
#   # ESP32 使用 Tensilica Xtensa LX6 或 RISC-V（ESP32-C3/C6）
#   # CPU flag 取决于 Xtensa / RISC-V gcc 工具链
#   CHIP_CPU   = -mcpu=esp32
#   CHIP_FPU   =
#   CHIP_DEFS  = -DCHIP_ESP32
#   CHIP_INCLUDES = \
#     -Ivendor/esp-idf/components/esp32/include \
#     -Ivendor/esp-idf/components/driver/include \
#     -Ivendor/esp-idf/components/esp_common/include
#   CHIP_HAL_SOURCES = \
#     vendor/esp-idf/components/esp32/source/*.c
#   CHIP_LD    = esp32.ld
#   CHIP_ASM   = startup.c  (ESP32 启动通常是 C)
# endif

# 未在表中列出 → 报错
ifeq ($(strip $(CHIP_CPU)),)
  $(error Unknown CHIP '$(CHIP)'. Check Makefile CHIP config table)
endif

# ─── 派生变量（所有路径以 CHIP_ROOT 为基准） ─────────────────────────────
CHIP_ROOT   = hal/chip/$(CHIP)
BUILD_DIR   = build

######################################
# source
######################################
# Chip layer —— ISA 实现 + 硬件 init + vendor HAL
# 注意 CHIP_HAL_SOURCES 的 -I/-L 前缀由 $(CHIP_ROOT)/ 统一拼接
CHIP_SOURCES := \
	$(CHIP_ROOT)/chip_init.c \
	$(CHIP_ROOT)/chip_irq.c \
	$(CHIP_ROOT)/hal_port_impl.c \
	$(CHIP_ROOT)/eeprom.c \
	$(CHIP_ROOT)/fal_stm32f1_port.c \
	$(CHIP_ROOT)/storage.c \
	$(CHIP_ROOT)/system_$(CHIP).c

# ─── Driver layer（完全不依赖 HAL） ───
DRIVER_SOURCES := \
	driver/src/util.c \
	driver/src/bldc.c

# ─── App layer（完全不依赖 HAL） ───
APP_SOURCES := \
	app/main.c \
	app/control.c \
	app/comms.c

# ─── MATLAB/Simulink generated ───
GEN_SOURCES := \
	gen/BLDC_controller.c \
	gen/BLDC_controller_data.c

# ─── FlashDB (third-party) ───
FLASHDB_SOURCES := \
	components/FlashDB/src/fdb.c \
	components/FlashDB/src/fdb_file.c \
	components/FlashDB/src/fdb_kvdb.c \
	components/FlashDB/src/fdb_tsdb.c \
	components/FlashDB/src/fdb_utils.c \
	components/FlashDB/port/fal/src/fal.c \
	components/FlashDB/port/fal/src/fal_flash.c \
	components/FlashDB/port/fal/src/fal_partition.c

# 所有 C 源 = 芯片适配 + 应用 + FlashDB + vendor HAL
# vendor HAL 源的相对路径相对于 $(CHIP_ROOT)/vendor/
# 所以这里要加 $(CHIP_ROOT)/vendor/ 前缀
C_SOURCES := \
	$(CHIP_SOURCES) \
	$(DRIVER_SOURCES) \
	$(APP_SOURCES) \
	$(GEN_SOURCES) \
	$(FLASHDB_SOURCES) \
	$(addprefix $(CHIP_ROOT)/,$(CHIP_HAL_SOURCES))

# 启动文件相对于 $(CHIP_ROOT)/
ASM_SOURCES = $(CHIP_ROOT)/$(CHIP_ASM)

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
AR = $(PREFIX)ar
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
MCU = $(CHIP_CPU) -mthumb $(CHIP_FPU)

AS_DEFS  =
C_DEFS   = $(CHIP_DEFS)

# 注意：CHIP_INCLUDES 里的 -I 路径也是相对于 $(CHIP_ROOT)/
# 所有 vendor HAL/CMSIS 头路径最终用 -I$(CHIP_ROOT)/xxx 拼接
C_INCLUDES = \
	-Iapp \
	-Iapp/config \
	-Idriver/inc \
	-Ihal/chip \
	-I$(CHIP_ROOT) \
	$(addprefix -I$(CHIP_ROOT)/,$(CHIP_INCLUDES)) \
	-I$(CHIP_ROOT)/vendor/CMSIS/Include \
	-Igen \
	-Icomponents/FlashDB/inc \
	-Icomponents/FlashDB/port/fal/inc

ASFLAGS = $(MCU) $(AS_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections
CFLAGS  = $(MCU) $(C_DEFS)  $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -std=gnu11

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)"

ifneq ($(VARIANT), )
CFLAGS += -D $(VARIANT)
endif

#######################################
# LDFLAGS
#######################################
LDSCRIPT = $(CHIP_ROOT)/$(CHIP_LD)
LIBS     = -lc -lm -lnosys
LIBDIR   =
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

#######################################
# build
#######################################
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c app/config/config.h Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s app/config/config.h Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

format:
	find app/ driver/ hal/ gen/ components/FlashDB \
	  -iname '*.h' -o -iname '*.c' | xargs clang-format -i

#######################################
# clean up
#######################################
clean:
	-rm -fR .dep build

flash:
	st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x8000000

unlock:
	openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg \
	  -c init -c "reset halt" -c "stm32f1x unlock 0"

#######################################
# dependencies
#######################################
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)

# *** EOF ***
