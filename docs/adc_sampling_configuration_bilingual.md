# ADC Sampling Configuration / ADC采样配置说明

## 1. Overall Architecture
## 1. 总体架构
The project uses dual-ADC regular simultaneous mode, where ADC1 is the master trigger path and ADC2 follows synchronously.
项目使用双ADC规则同步采样模式，ADC1作为主触发路径，ADC2同步跟随。
The dual mode is configured by setting ADC_DUALMODE_REGSIMULT and applying HAL_ADCEx_MultiModeConfigChannel on ADC1.
双模通过设置ADC_DUALMODE_REGSIMULT并在ADC1上调用HAL_ADCEx_MultiModeConfigChannel完成配置。
The trigger chain is TIM1 -> TIM8 -> ADC, so current sampling stays aligned with PWM timing.
触发链路为TIM1 -> TIM8 -> ADC，从而保证电流采样与PWM时序对齐。
ADC clock is configured to APB2 divider /4 (about 16 MHz in this project).
ADC时钟配置为APB2分频/4（本项目约16 MHz）。

## 2. ADC1 Configuration (Master)
## 2. ADC1配置（主ADC）
ADC1 uses scan mode with 5 regular conversions, external trigger from TIM8 TRGO, and right alignment.
ADC1使用扫描模式，常规组5次转换，外部触发源为TIM8 TRGO，数据右对齐。
Rank1 is CH11 (PC1), Rank2 is CH0 (PA0), and Rank3 is CH14 (PC4).
Rank1为CH11（PC1），Rank2为CH0（PA0），Rank3为CH14（PC4）。
Rank4 is battery input: CH12/PC2 for BOARD_VARIANT 0, or CH1/PA1 for BOARD_VARIANT 1.
Rank4为电池输入：BOARD_VARIANT 0使用CH12/PC2，BOARD_VARIANT 1使用CH1/PA1。
Rank5 is internal temperature sensor with long sampling time for stability.
Rank5为内部温度传感器，并使用较长采样时间以保证稳定性。

## 3. ADC2 Configuration (Follower)
## 3. ADC2配置（从ADC）
ADC2 also uses scan mode with 5 regular conversions.
ADC2同样使用扫描模式，常规组5次转换。
ADC2 ExternalTrigConv is set to ADC_SOFTWARE_START as a placeholder in this synchronized dual-mode design.
在该同步双模设计中，ADC2的ExternalTrigConv设置为ADC_SOFTWARE_START作为占位配置。
Actual conversion timing is still determined by ADC1 external trigger and the dual synchronous mechanism.
实际转换节拍仍由ADC1外部触发与双模同步机制决定。
ADC2 channels are CH10 (PC0), CH13 (PC3), CH15 (PC5), CH2 (PA2), and CH3 (PA3).
ADC2通道依次为CH10（PC0）、CH13（PC3）、CH15（PC5）、CH2（PA2）和CH3（PA3）。

## 4. Sampling Time Strategy
## 4. 采样时间策略
Most current-related channels use short sampling times (1.5 or 7.5 cycles) to keep control-loop latency low.
多数电流相关通道使用较短采样时间（1.5或7.5周期）以降低控制环延迟。
The internal temperature channel uses 239.5 cycles to satisfy minimum sampling requirements.
内部温度通道使用239.5周期以满足最小采样时间要求。

## 5. DMA Configuration and Data Packing
## 5. DMA配置与数据打包
DMA1 Channel1 transfers from ADC1->DR into adc_buffer in circular mode.
DMA1 Channel1以循环模式将ADC1->DR搬运到adc_buffer。
CNDTR is 5, meaning each DMA round transfers 5 packed 32-bit samples.
CNDTR为5，表示每轮DMA搬运5个打包的32位样本。
MSIZE and PSIZE are both set to 32-bit because each ADC1 DR read contains packed ADC1+ADC2 data in dual mode.
MSIZE和PSIZE都设为32位，因为在双模下每次读取ADC1 DR都包含打包后的ADC1+ADC2数据。
MINC enables memory address increment, CIRC enables continuous looping, and TCIE enables transfer-complete interrupt.
MINC使能内存地址递增，CIRC使能循环，TCIE使能传输完成中断。

## 6. Runtime Processing Path
## 6. 运行时处理路径
Both ADC1 and ADC2 are started during main initialization.
ADC1和ADC2都在主初始化阶段启动。
DMA transfer-complete interrupt enters DMA1_Channel1_IRQHandler in bldc.c.
DMA传输完成中断进入bldc.c中的DMA1_Channel1_IRQHandler。
The first 2000 iterations are used for ADC offset calibration.
前2000次迭代用于ADC零偏校准。
After calibration, phase currents and DC-link currents are computed every cycle, and protection logic is applied immediately.
校准完成后，每个周期都会计算相电流与母线电流，并立即执行保护逻辑。
Battery voltage is low-pass filtered at a slower rate to improve robustness.
电池电压以较低速率进行低通滤波以提升稳健性。

## 7. Why ADC2 DMA Is Not Configured Separately
## 7. 为什么不单独配置ADC2的DMA
In dual regular simultaneous mode, ADC2 results are merged into ADC1 data path.
在双ADC规则同步模式下，ADC2结果会并入ADC1数据路径。
Therefore one DMA channel on ADC1 is enough to capture both ADC results per sample instant.
因此只需配置ADC1侧的一条DMA通道即可在每个采样时刻同时获取两路ADC结果。
A separate ADC2 DMA path is only required if ADC1/ADC2 are switched to independent modes.
只有在将ADC1/ADC2改为独立模式时，才需要单独的ADC2 DMA路径。

## 8. Key Source References
## 8. 关键源码定位
Dual mode setup: Src/setup.c lines around ADC_DUALMODE_REGSIMULT and HAL_ADCEx_MultiModeConfigChannel.
双模配置位置：Src/setup.c中ADC_DUALMODE_REGSIMULT与HAL_ADCEx_MultiModeConfigChannel附近代码。
ADC1 trigger source: Src/setup.c around ADC_EXTERNALTRIGCONV_T8_TRGO.
ADC1触发源：Src/setup.c中ADC_EXTERNALTRIGCONV_T8_TRGO附近代码。
DMA settings: Src/setup.c around DMA1_Channel1 CNDTR/CPAR/CMAR/CCR configuration.
DMA设置：Src/setup.c中DMA1_Channel1的CNDTR/CPAR/CMAR/CCR配置附近代码。
Runtime use: Src/bldc.c in DMA1_Channel1_IRQHandler.
运行时使用：Src/bldc.c中的DMA1_Channel1_IRQHandler。
