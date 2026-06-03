#ifndef BOARD_STORAGE_H__
#define BOARD_STORAGE_H__


#include "stdint.h"

void storage_init(void);
int storage_read(char *key, uint8_t *buf, uint32_t size);
int storage_write(char *key, uint8_t *buf, uint32_t size);

#endif // !BOARD_STORAGE_H__