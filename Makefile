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

# Build path
BUILD_DIR = build

######################################
# source
######################################
# Source definitions by architectural layer
# ────────────────────────────────────────
#  HAL layer       hal/stm32         hardware init, interrupts, HAL config
#  Driver layer    driver/           functional modules (motor, storage, util, comms...)
#  App layer       app/              business logic, entry point, configuration
#  Gen             gen/              MATLAB/Simulink auto-generated
#  Components      components/       third-party (FlashDB, MultiButton)
#  Drivers         Drivers/          STM32 HAL + CMSIS (official, unmodified)
# ────────────────────────────────────────

# HAL layer — hardware abstraction (STM32F1xx board bring-up)
HAL_SOURCES := \
	hal/stm32/src/system_stm32f1xx.c \
	hal/stm32/src/setup.c \
	hal/stm32/src/stm32f1xx_it.c

# Driver layer — reusable functional modules
DRIVER_SOURCES := \
	driver/src/util.c \
	driver/src/bldc.c \
	driver/src/eeprom.c \
	driver/src/storage.c \
	driver/src/fal_stm32f1_port.c

# App layer — application entry + comms protocol
APP_SOURCES := \
	app/main.c \
	app/control.c \
	app/comms.c

# Generated code (Simulink/Codegen)
GEN_SOURCES := \
	gen/BLDC_controller.c \
	gen/BLDC_controller_data.c

# Third-party storage engine (FlashDB + FAL)
FLASHDB_SOURCES := \
	components/FlashDB/src/fdb.c \
	components/FlashDB/src/fdb_file.c \
	components/FlashDB/src/fdb_kvdb.c \
	components/FlashDB/src/fdb_tsdb.c \
	components/FlashDB/src/fdb_utils.c \
	components/FlashDB/port/fal/src/fal.c \
	components/FlashDB/port/fal/src/fal_flash.c \
	components/FlashDB/port/fal/src/fal_partition.c

# STM32 HAL drivers (official vendor library)
STM32_HAL_SOURCES := \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c \
	Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c

# Combine all sources
C_SOURCES := \
	$(HAL_SOURCES) \
	$(DRIVER_SOURCES) \
	$(APP_SOURCES) \
	$(GEN_SOURCES) \
	$(FLASHDB_SOURCES) \
	$(STM32_HAL_SOURCES)

# ASM sources
ASM_SOURCES =  \
startup_stm32f103xe.s

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
#PREFIX = gcc-arm-none-eabi-7/bin/arm-none-eabi-
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
# cpu
CPU = -mcpu=cortex-m3

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# macros for gcc
AS_DEFS =

# C defines
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DSTM32F103xE

# AS includes
AS_INCLUDES =

# C includes — ordered by layer (hal → driver → app → gen → vendors)
C_INCLUDES =  \
-Iapp \
-Iapp/config \
-Idriver/inc \
-Ihal/stm32/include \
-Igen \
-IDrivers/STM32F1xx_HAL_Driver/Inc \
-IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
-IDrivers/CMSIS/Include \
-Icomponents/FlashDB/inc \
-Icomponents/FlashDB/port/fal/inc

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -std=gnu11

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)"

# Choose variant from env var
# make -e VARIANT=VARIANT_ADC

ifneq ($(VARIANT), )
CFLAGS += -D $(VARIANT)
endif


#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = STM32F103RCTx_FLASH.ld

# libraries
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
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
	find app/ driver/ hal/ gen/ components/FlashDB -iname '*.h' -o -iname '*.c' | xargs clang-format -i
#######################################
# clean up
#######################################
clean:
	-rm -fR .dep $(BUILD_DIR)

flash:
	st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x8000000

unlock:
	openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -c init -c "reset halt" -c "stm32f1x unlock 0"

#######################################
# dependencies
#######################################
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)

# *** EOF ***
