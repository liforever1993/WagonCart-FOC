#include <string.h>
#include <fal.h>
#include <stm32f1xx_hal.h>

#if defined(STM32F103xE)
#define FLASH_PAGE_BYTES 2048U
#else
#define FLASH_PAGE_BYTES 1024U
#endif

static int stm32_flash_init(void) {
    return 1;
}

static int stm32_flash_read(long offset, uint8_t *buf, size_t size) {
    uint32_t addr = stm32_onchip_flash.addr + (uint32_t)offset;

    for (size_t i = 0; i < size; i++) {
        buf[i] = *(const uint8_t *)(addr + i);
    }

    return (int)size;
}

static int stm32_flash_write(long offset, const uint8_t *buf, size_t size) {
    uint32_t addr = stm32_onchip_flash.addr + (uint32_t)offset;

    if ((addr % 4U) != 0U || (size % 4U) != 0U) {
        return -1;
    }

    HAL_FLASH_Unlock();

    for (size_t i = 0; i < size; i += 4U) {
        uint32_t word;
        memcpy(&word, &buf[i], sizeof(word));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (uint32_t)i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
    }

    HAL_FLASH_Lock();
    return (int)size;
}

static int stm32_flash_erase(long offset, size_t size) {
    uint32_t addr = stm32_onchip_flash.addr + (uint32_t)offset;
    uint32_t page_error = 0U;
    FLASH_EraseInitTypeDef erase = {0};

    size_t pages = size / FLASH_PAGE_BYTES;
    if ((size % FLASH_PAGE_BYTES) != 0U) {
        pages++;
    }

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.NbPages = 1;

    HAL_FLASH_Unlock();

    for (size_t i = 0; i < pages; i++) {
        erase.PageAddress = addr + (uint32_t)(FLASH_PAGE_BYTES * i);
        if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
    }

    HAL_FLASH_Lock();
    return (int)size;
}

const struct fal_flash_dev stm32_onchip_flash = {
    .name = "stm32_onchip",
    .addr = 0x08000000,
    .len = 256 * 1024,
    .blk_size = 2 * 1024,
    .ops = {stm32_flash_init, stm32_flash_read, stm32_flash_write, stm32_flash_erase},
    .write_gran = 32,
};
