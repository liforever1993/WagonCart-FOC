/*
* This file is part of the hoverboard-firmware-hack project.
*
* Copyright (C) 2017-2018 Rene Hopf <renehopf@mac.com>
* Copyright (C) 2017-2018 Nico Stute <crinq@crinq.de>
* Copyright (C) 2017-2018 Niklas Fauth <niklas.fauth@kit.fail>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Define to prevent recursive inclusion
#ifndef CHIP_INIT_H
#define CHIP_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================================
 * 系统级初始化 —— 把 main() 开头的 HAL_Init / 时钟 / NVIC 全封在 chip 层
 * ===================================================================== */
void chip_system_init(void);       /* HAL_Init + NVIC 优先级分组 + SystemClock_Config */
void chip_peripheral_init(void);   /* MX_GPIO_Init + MX_TIM_Init + MX_ADC1/2_Init + UART/DMA */
void chip_adc_start(void);         /* HAL_ADC_Start(&hadc1/2) */

/* =====================================================================
 * 外设初始化函数（仍暴露，供 chip 层内部或特殊场景调用）
 * ===================================================================== */
void MX_GPIO_Init(void);
void MX_TIM_Init(void);
void MX_ADC1_Init(void);
void MX_ADC2_Init(void);
void UART2_Init(void);
void UART3_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* CHIP_INIT_H */
