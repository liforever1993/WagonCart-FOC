#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#define FAL_PART_HAS_TABLE_CFG

/* ===================== Flash Device Configuration ====================== */
extern const struct fal_flash_dev stm32_onchip_flash;

#define FAL_FLASH_DEV_TABLE               \
{                                         \
    &stm32_onchip_flash,                  \
}

/* ======================= Partition Configuration ======================= */
#ifdef FAL_PART_HAS_TABLE_CFG
// Magic word, partition name, flash device name, offsert, size, flags
#define FAL_PART_TABLE                                                    \
{                                                                         \
    {FAL_PART_MAGIC_WORD, "flashdb", "stm32_onchip", 192 * 1024, 64 * 1024, 0}, \
}
#endif

#endif /* _FAL_CFG_H_ */
