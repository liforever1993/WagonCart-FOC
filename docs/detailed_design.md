# hoverboard-firmware-hack-FOC Detailed Design / 详细设计
# hoverboard-firmware-hack-FOC 详细设计 / Detailed Design

## 1. Document Purpose
## 1. 文档目的
This document describes the firmware detailed design with focus on runtime behavior, module boundaries, data flow, safety, and configuration.
本文档描述固件详细设计，重点包括运行时行为、模块边界、数据流、安全机制与配置模型。
This design is derived from the current repository source and build settings.
本设计基于当前仓库源码和构建配置提炼。

## 2. System Context
## 2. 系统背景
The firmware targets STM32F103-class hoverboard mainboards and controls two BLDC motors.
固件面向 STM32F103 系列平衡车主板，控制两路 BLDC 电机。
Supported control families are Commutation, Sinusoidal, and FOC.
支持的控制族包括换相控制、正弦控制和 FOC。
FOC supports Voltage mode, Speed mode, and Torque mode.
FOC 支持电压模式、速度模式和扭矩模式。

### 2.1 Main Hardware Interfaces
### 2.1 主要硬件接口
Two inverter bridges are driven by TIM1 and TIM8 PWM outputs.
两路逆变桥分别由 TIM1 和 TIM8 的 PWM 输出驱动。
Hall sensors provide rotor state feedback for both motors.
两侧霍尔传感器提供转子状态反馈。
ADC channels sample phase currents, DC link currents, battery voltage, and board temperature.
ADC 通道采样相电流、母线电流、电池电压和板载温度。
Optional control inputs include ADC, UART, PPM, PWM, iBUS, and Nunchuk.
可选控制输入包括 ADC、UART、PPM、PWM、iBUS 和 Nunchuk。
Optional sideboard communication is supported through UART links.
可选 sideboard 通信通过 UART 链路实现。

## 3. Software Architecture
## 3. 软件架构
The software is organized into four layers.
软件分为四个层次。
Layer 1 is platform and peripherals (clock, GPIO, TIM, ADC, UART, I2C, DMA, IRQ).
第 1 层是平台与外设层（时钟、GPIO、TIM、ADC、UART、I2C、DMA、中断）。
Layer 2 is control execution in fast interrupt context.
第 2 层是快速中断上下文中的控制执行层。
Layer 3 is application logic for input processing, policies, and system behavior.
第 3 层是输入处理、策略与系统行为的应用逻辑层。
Layer 4 is configuration and persistence (compile-time macros and EEPROM emulation).
第 4 层是配置与持久化层（编译期开关与 EEPROM 仿真）。

## 4. Build and Deployment
## 4. 构建与部署
The primary build entry is Makefile.
主要构建入口为 Makefile。
The toolchain is arm-none-eabi-gcc with gnu11.
工具链为 arm-none-eabi-gcc，语言标准 gnu11。
The linker uses nano.specs, nosys, and STM32F103RCTx_FLASH.ld.
链接使用 nano.specs、nosys 和 STM32F103RCTx_FLASH.ld。
Outputs are generated under build/ as ELF, HEX, and BIN.
输出在 build/ 目录生成 ELF、HEX 和 BIN 文件。
Flashing is provided through st-flash targets.
烧录通过 st-flash 目标提供。

## 5. Runtime Execution Model
## 5. 运行时执行模型
The firmware uses a dual-rate execution strategy.
固件采用双速率执行策略。
A fast path runs in DMA1_Channel1_IRQHandler at about 16 kHz.
快速路径在 DMA1_Channel1_IRQHandler 中运行，频率约 16 kHz。
A slower application path runs in the main loop with periodic gating.
较慢的应用路径在主循环中按周期门控执行。

### 5.1 Fast Path Responsibilities
### 5.1 快速路径职责
Calibrate ADC offsets during startup window.
在启动窗口内完成 ADC 零偏标定。
Acquire phase/DC current and battery measurements.
采集相电流、母线电流和电池电压。
Apply Level-2 current chopping by toggling MOE when needed.
在需要时通过 MOE 门控执行二级电流 chopping 保护。
Execute BLDC_controller_step for left and right motors.
分别执行左右电机 BLDC_controller_step。
Update timer compare registers for three-phase PWM outputs.
更新三相 PWM 对应的定时器比较寄存器。

### 5.2 Main Loop Responsibilities
### 5.2 主循环职责
Read and normalize commands from active input source.
读取并归一化当前输入源命令。
Apply optional features such as cruise, standstill hold, and electric brake.
应用可选功能，如巡航、驻停保持和电刹。
Perform command shaping using rate limiter, low-pass filter, and mixer.
使用限速、低通与混控完成命令整形。
Send optional debug and feedback telemetry frames.
发送可选调试与反馈遥测帧。
Evaluate poweroff conditions and warning beeps.
评估关机条件与告警蜂鸣。
The main loop owns product behavior decisions, while ISR owns hard real-time actuation.
主循环负责产品行为决策，ISR 负责硬实时执行。

## 6. Data Flow
## 6. 数据流
Input data enters through source-specific handlers and readInputRaw().
输入数据通过各输入处理器与 readInputRaw() 进入系统。
readCommand() maps raw values into command space and enforces safety fallbacks.
readCommand() 将原始值映射到命令空间并执行安全回退。
Command values are transformed into left/right targets by mixer logic.
命令经混控逻辑转换为左右电机目标。
Targets become pwml/pwmr and are consumed in the fast ISR control loop.
目标映射为 pwml/pwmr，并在快速 ISR 控制环中消费。
Controller outputs are converted to three-phase duty cycles and written to hardware timers.
控制器输出被转换为三相占空比并写入硬件定时器。

### 6.1 End-to-End Path (One Control Cycle)
### 6.1 端到端路径（单次控制周期）
Input sample is captured (ADC/UART/PPM/PWM/iBUS/Nunchuk).
采集输入样本（ADC/UART/PPM/PWM/iBUS/Nunchuk）。
Input is normalized to command range and validated by timeout logic.
输入被归一化到命令范围，并经过超时有效性检查。
Feature policies may modify command (cruise, hold, brake, variant logic).
功能策略可能改写命令（巡航、驻停保持、电刹、变体逻辑）。
Command is shaped (rate limit + LPF + mixer) into left/right target.
命令经整形（限速 + 低通 + 混控）得到左右目标。
Fast ISR consumes targets, executes controller, and writes PWM duty.
快速 ISR 消费目标，执行控制器并写入 PWM 占空比。

### 6.2 Functional Scenarios
### 6.2 功能场景
Scenario A: Power-on and arming.
场景 A：上电与解锁使能。
Trigger: system boots and user command is near neutral.
触发：系统上电且用户命令接近中位。
Expected behavior: latch power, initialize IO/control, enable motors only after neutral check.
期望行为：锁存供电、初始化 IO/控制，仅在中位检查通过后使能电机。
Safety result: prevents accidental torque on startup.
安全结果：防止上电瞬间误扭矩。

Scenario B: Normal driving.
场景 B：正常行驶。
Trigger: valid input stream and no diagnostics/timeouts.
触发：输入流有效且无诊断错误/超时。
Expected behavior: command tracking according to selected mode and limits.
期望行为：按所选模式与限值进行命令跟踪。
Safety result: current and speed are constrained by configured caps.
安全结果：电流与速度受配置上限约束。

Scenario C: Hill stall or overload.
场景 C：坡道顶不动或过载。
Trigger: requested output remains high while speed stays very low.
触发：请求输出持续较高但转速长期很低。
Expected behavior: first hit i_max limit, then diagnostics may raise error and force safe fallback.
期望行为：先触发 i_max 限流，随后可能触发诊断错误并回退安全模式。
Safety result: avoids unlimited current demand and uncontrolled heating.
安全结果：避免无限增流和不可控发热。

Scenario D: Input link loss.
场景 D：输入链路丢失。
Trigger: ADC/serial/general timeout flags become active.
触发：ADC/串口/通用输入超时标志置位。
Expected behavior: force OPEN_MODE request and zero-safe command.
期望行为：强制 OPEN_MODE 请求并输出安全零命令。
Safety result: controlled coast-down instead of stale command hold.
安全结果：受控卸力而非继续执行陈旧命令。

Scenario E: Thermal or battery protection.
场景 E：温度或电池保护。
Trigger: over-temperature or low-voltage thresholds are crossed under configured conditions.
触发：满足配置条件时越过过温或欠压阈值。
Expected behavior: warning beeps first, then controlled poweroff when required.
期望行为：先告警蜂鸣，必要时执行受控关机。
Safety result: reduces damage risk to board, battery, and drivetrain.
安全结果：降低主板、电池与传动系统损伤风险。

## 7. Safety and Protection
## 7. 安全与保护
Protection is implemented as layered mechanisms.
保护机制采用分层设计。
Level-1 limits motor current demand with i_max in control logic.
一级保护在控制逻辑中通过 i_max 限制电机电流需求。
Level-2 disables bridge outputs via MOE when DC current exceeds threshold.
二级保护在母线电流超阈值时通过 MOE 关断桥臂输出。
Diagnostics can raise z_errCode and force safe mode behavior.
诊断可置位 z_errCode 并触发安全模式行为。
Input timeouts force OPEN_MODE request and zero-safe commands.
输入超时会强制 OPEN_MODE 请求并输出安全零命令。
Thermal, battery, and inactivity logic can trigger controlled poweroff.
温度、电池和静置超时逻辑可触发受控关机。

## 8. Module-Level Design
## 8. 模块级设计
main.c
main.c
Trigger: firmware boot and periodic main-loop tick.
触发：固件上电和主循环周期执行。
Inputs: normalized commands, state flags, diagnostics, battery/temperature values.
输入：归一化命令、状态标志、诊断结果、电池与温度值。
Outputs: final pwml/pwmr targets, beeps, telemetry, and poweroff decisions.
输出：最终 pwml/pwmr 目标、蜂鸣、遥测及关机决策。

bldc.c
bldc.c
Trigger: DMA1 Channel1 interrupt at control frequency.
触发：DMA1 Channel1 控制频率中断。
Inputs: ADC currents/voltage, hall states, ctrl mode request, pwml/pwmr.
输入：ADC 电流/电压、霍尔状态、控制模式请求、pwml/pwmr。
Outputs: timer compare values, MOE gate state, updated controller outputs.
输出：定时器比较值、MOE 门控状态、更新后的控制器输出。

util.c
util.c
Trigger: called by main loop and UART IDLE callbacks.
触发：由主循环和 UART IDLE 回调调用。
Inputs: raw input channels and feature configuration.
输入：原始输入通道与功能配置。
Outputs: command shaping results, timeout flags, optional feature state transitions.
输出：命令整形结果、超时标志、可选功能状态切换。

control.c
control.c
Trigger: EXTI and SysTick events for PPM/PWM/Nunchuk supervision.
触发：PPM/PWM/Nunchuk 相关 EXTI 与 SysTick 事件。
Inputs: edge timing and I2C/UART side data.
输入：边沿时序与 I2C/UART 侧数据。
Outputs: decoded channel values and general timeout health.
输出：解码后的通道值与通用超时健康状态。

setup.c
setup.c
Trigger: startup initialization sequence.
触发：启动初始化流程。
Inputs: compile-time feature macros.
输入：编译期功能宏。
Outputs: initialized peripheral instances and DMA mappings.
输出：已初始化外设实例与 DMA 映射。

stm32f1xx_it.c
stm32f1xx_it.c
Trigger: MCU exceptions and peripheral interrupts.
触发：MCU 异常与外设中断。
Inputs: IRQ events and status flags.
输入：中断事件与状态标志。
Outputs: callback dispatch to control/input/communication handlers.
输出：分发回调到控制、输入和通信处理器。

comms.c
comms.c
Trigger: debug serial protocol processing path (optional).
触发：调试串口协议处理路径（可选）。
Inputs: ASCII command stream.
输入：ASCII 命令流。
Outputs: parameter updates, watch output, help/status responses.
输出：参数更新、watch 输出、帮助/状态响应。

eeprom.c
eeprom.c
Trigger: configuration load/save operations.
触发：配置加载/保存操作。
Inputs: virtual addresses and parameter data.
输入：虚拟地址与参数数据。
Outputs: flash page writes/reads with recovery handling.
输出：带恢复处理的 Flash 页读写。

BLDC_controller.c/.data.c
BLDC_controller.c/.data.c
Trigger: one step per fast control interrupt.
触发：每次快速控制中断执行一步。
Inputs: motor state, command, currents, hall signals, parameters.
输入：电机状态、命令、电流、霍尔信号、参数。
Outputs: phase duty references, speed estimate, and diagnostic code.
输出：相占空比参考、速度估计和诊断码。

## 9. Configuration Model
## 9. 配置模型
Compile-time configuration is driven by config.h macros.
编译期配置由 config.h 宏驱动。
Macros control variants, control defaults, limits, optional features, and pin/resource checks.
宏控制变体、控制默认值、限制参数、可选功能及引脚/资源冲突检查。
The current profile in repository uses VARIANT_PWM with FOC + SPD_MODE.
当前仓库配置使用 VARIANT_PWM，控制为 FOC + SPD_MODE。
Current limits and diagnostics are enabled by default in active settings.
当前生效配置默认启用电流限制和诊断。

## 10. Timing and Determinism
## 10. 时序与确定性
Control determinism is anchored by interrupt-driven fast loop.
控制确定性由中断驱动的快速环保障。
Main loop timing is best-effort and feature-dependent.
主循环时序为尽力而为，受功能开关影响。
OverrunFlag in fast ISR prevents re-entrant controller execution.
快速 ISR 中的 OverrunFlag 用于防止控制器重入。

## 11. Communication Design
## 11. 通信设计
Binary control packets use start frame and checksum validation.
二进制控制包使用起始帧与校验进行有效性验证。
DMA circular RX plus UART IDLE interrupt provides framing trigger.
DMA 环形接收结合 UART IDLE 中断完成分帧触发。
Feedback packets include command echo, speed, battery, temperature, LED field, and checksum.
反馈包包含命令回显、速度、电压、温度、LED 位域和校验。
Optional debug text protocol supports GET, SET, INIT, SAVE, WATCH, and HELP.
可选调试文本协议支持 GET、SET、INIT、SAVE、WATCH、HELP。

## 12. Non-Functional Notes
## 12. 非功能说明
The code is hardware-coupled to STM32F103 and board pin assumptions.
代码与 STM32F103 及板级引脚假设耦合较高。
Maintainability relies on keeping generated controller code as stable core.
可维护性依赖将生成控制器代码作为稳定核心。
Policy and adaptation should stay in hand-written application layers.
策略和适配逻辑应保持在手写应用层。
System safety also depends on mechanical design, BMS behavior, and calibration quality.
系统安全还依赖机械设计、BMS 行为与标定质量。

## 13. Extension Points
## 13. 扩展点
Add new input sources in readInputRaw() and connect timeout paths.
可在 readInputRaw() 中新增输入源并接入超时路径。
Add new policies in main loop before or after mixer stage.
可在主循环混控前后添加新策略。
Expose runtime tunables through comms.c parameter table.
可通过 comms.c 参数表暴露运行时可调参数。
Tune controller parameters in BLDC_controller_data.c or runtime protocol.
控制参数可在 BLDC_controller_data.c 或运行时协议中调优。

## 14. Constraints and Risks
## 14. 约束与风险
Complex macro combinations can create subtle behavior interactions.
复杂宏组合可能产生隐蔽行为联动。
Shared physical pins among features require strict compile-time conflict resolution.
多功能共享物理引脚，必须严格通过编译期冲突检查配置。
Mode transitions are distributed between generated core and hand-written logic.
模式切换逻辑分布在生成核心与手写逻辑之间。
Troubleshooting often requires cross-file trace analysis.
问题排查通常需要跨文件链路追踪。

## 15. Verification Recommendations
## 15. 验证建议
Verify build for selected variant and target toolchain.
验证所选变体与目标工具链构建通过。
Validate startup, arming, and idle behavior before load tests.
负载测试前先验证上电、解锁与空载行为。
Validate input validity and timeout fallback behavior.
验证输入有效性与超时回退行为。
Validate current limiting and diagnostics under realistic load.
在真实负载下验证限流与诊断行为。
Validate thermal and undervoltage warning/shutdown paths.
验证温度与欠压告警/关机路径。
Validate inactivity shutdown and telemetry integrity.
验证静置关机与遥测完整性。

## 16. Config Parameter Reference
## 16. config 参数作用速查
This section explains the role of configurable macros in Inc/config.h.
本节按 Inc/config.h 分区解释可配置宏的作用。
Validation-only checks and error guards are not repeated here.
仅用于校验的编译检查和 error 宏不在本节逐条展开。

### 16.1 Variant Selection
### 16.1 变体选择
- VARIANT_ADC: 双电位器 ADC 控制方案。
- VARIANT_USART: 串口控制方案。
- VARIANT_NUNCHUK: Nunchuk 控制方案。
- VARIANT_PPM: 遥控 PPM 控制方案。
- VARIANT_PWM: 遥控 PWM 控制方案。
- VARIANT_IBUS: FlySky iBUS 控制方案。
- VARIANT_HOVERCAR: Hovercar 专用配置。
- VARIANT_HOVERBOARD: Hoverboard 原生通信/平衡扩展配置。
- VARIANT_TRANSPOTTER: TranspOtter 专用配置。
- VARIANT_SKATEBOARD: Skateboard 专用配置。

### 16.2 Core Timing And Conversion
### 16.2 基础时序与换算
- PWM_FREQ: PWM 和蜂鸣器基准频率。
- DEAD_TIME: 三相桥臂死区时间。
- DELAY_IN_MAIN_LOOP: 主循环步长，影响低速任务调度节拍。
- TIMEOUT: 通用输入超时计数阈值。
- A2BIT_CONV: 电流安培到 ADC bit 的换算系数。
- PRINTF_FLOAT_SUPPORT: 允许串口 printf 输出浮点，增大代码体积。
- ADC_CONV_TIME_1C5 到 ADC_CONV_TIME_239C5: ADC 单次转换时钟周期预定义。
- ADC_CONV_CLOCK_CYCLES: 当前使用的 ADC 采样周期配置。
- ADC_CLOCK_DIV: ADC 分频设置。
- ADC_TOTAL_CONV_TIME: 由分频和采样周期得到的总转换时间，用于相位对齐。

### 16.3 Board Variant
### 16.3 板型差异
- BOARD_VARIANT: 主板引脚映射选择。0 为默认板，1 为替代引脚板。

### 16.4 Battery Parameters
### 16.4 电池参数
- BAT_FILT_COEF: 电池电压低通滤波系数。
- BAT_CALIB_REAL_VOLTAGE: 外部实测电池电压标定值。
- BAT_CALIB_ADC: 与上项对应的 ADC 原始值。
- BAT_CELLS: 电池串联节数。
- BAT_LVL2_ENABLE: 二级低电压蜂鸣开关。
- BAT_LVL1_ENABLE: 一级低电压蜂鸣开关。
- BAT_DEAD_ENABLE: 严重欠压关机开关。
- BAT_BLINK_INTERVAL: 电池灯闪烁节拍。
- BAT_LVL5/BAT_LVL4/BAT_LVL3/BAT_LVL2/BAT_LVL1/BAT_DEAD: 各级电压阈值。

### 16.5 Temperature Parameters
### 16.5 温度参数
- TEMP_FILT_COEF: 温度低通滤波系数。
- TEMP_CAL_LOW_ADC/TEMP_CAL_HIGH_ADC: 两点温度 ADC 标定值。
- TEMP_CAL_LOW_DEG_C/TEMP_CAL_HIGH_DEG_C: 两点温度实测值。
- TEMP_WARNING_ENABLE: 过温预警蜂鸣开关。
- TEMP_WARNING: 过温预警阈值。
- TEMP_POWEROFF_ENABLE: 过温关机开关。
- TEMP_POWEROFF: 过温关机阈值。

### 16.6 Motor Control Parameters
### 16.6 电机控制参数
- COM_CTRL/SIN_CTRL/FOC_CTRL: 控制器类型枚举。
- OPEN_MODE/VLT_MODE/SPD_MODE/TRQ_MODE: 控制模式枚举。
- MOTOR_LEFT_ENA/MOTOR_RIGHT_ENA: 左右电机使能。
- HALL_MAP_60_DEG: 使用 60 度霍尔映射表。
- CTRL_TYP_SEL: 默认控制类型选择。
- CTRL_MOD_REQ: 默认控制模式请求。
- DIAG_ENA: 诊断使能，决定是否上报码并触发保护逻辑。
- I_MOT_MAX: 单电机相电流上限。
- I_DC_MAX: 母线电流二级 chopping 阈值。
- N_MOT_MAX: 电机转速上限。
- FIELD_WEAK_ENA: 弱磁/相位提前使能。
- FIELD_WEAK_MAX: FOC 弱磁最大 d 轴电流。
- PHASE_ADV_MAX: SIN 模式最大相位提前角。
- FIELD_WEAK_HI/FIELD_WEAK_LO: 弱磁进入与满量程区间阈值。
- STANDSTILL_HOLD_ENABLE: 驻停保持开关。
- ELECTRIC_BRAKE_ENABLE: 电刹功能开关。
- ELECTRIC_BRAKE_MAX: 电刹最大幅值。
- ELECTRIC_BRAKE_THRES: 电刹生效阈值。

### 16.7 Default And Safety Parameters
### 16.7 默认与安全参数
- INACTIVITY_TIMEOUT: 长时间无动作自动关机分钟数。
- BEEPS_BACKWARD: 倒车蜂鸣开关。
- ADC_MARGIN: ADC 自动标定后保留边界。
- ADC_PROTECT_TIMEOUT: ADC 异常持续计数阈值。
- ADC_PROTECT_THRESH: ADC 输入越界判定阈值。
- AUTO_CALIBRATION_ENA: 长按按键进入输入自动标定开关。
- DEFAULT_RATE: 默认加减速限幅速率。
- DEFAULT_FILTER: 默认输入低通滤波系数。
- DEFAULT_SPEED_COEFFICIENT: 默认油门比例系数。
- DEFAULT_STEER_COEFFICIENT: 默认转向比例系数。
- RATE/FILTER/SPEED_COEFFICIENT/STEER_COEFFICIENT: 若变体未覆盖则使用上述默认值。
- INPUTS_NR: 单输入或双输入通道数量自动推导值。

### 16.8 Input Format Parameters
### 16.8 输入格式参数
- PRI_INPUT1/PRI_INPUT2: 主输入通道 1/2 的类型和量程定义。
- AUX_INPUT1/AUX_INPUT2: 辅输入通道 1/2 的类型和量程定义。
- TYPE: 输入类型编码。0 禁用，1 常规电位器，2 回中电位器，3 自动识别。
- MIN/MID/MAX: 输入最小、中位、最大标定值。
- DEADBAND: 中位死区范围。

### 16.9 Cruise, Debug, Buzzer, LCD
### 16.9 巡航、调试、蜂鸣与 LCD
- CRUISE_CONTROL_SUPPORT: 巡航控制功能总开关。
- SUPPORT_BUTTONS_LEFT/SUPPORT_BUTTONS_RIGHT/SUPPORT_BUTTONS: 按键来源配置。
- DEBUG_SERIAL_USART2/DEBUG_SERIAL_USART3: 文本调试串口开关。
- DEBUG_SERIAL_PROTOCOL: 串口调试命令协议开关。
- DEBUG_I2C_LCD: I2C 字符 LCD 调试显示开关。
- BUZZER_ENABLED: 蜂鸣器总开关。

### 16.10 UART Parameters
### 16.10 串口参数
- SERIAL_START_FRAME: 二进制串口协议帧头。
- SERIAL_BUFFER_SIZE: DMA 接收缓冲区大小。
- SERIAL_TIMEOUT: 串口命令超时阈值。
- USART2_BAUD/USART3_BAUD: UART2/UART3 波特率。
- USART2_WORDLENGTH/USART3_WORDLENGTH: UART 字长配置。

### 16.11 Variant ADC Key Parameters
### 16.11 VARIANT_ADC 关键参数
- CONTROL_ADC: 启用 ADC 输入并设定主辅优先级。
- DUAL_INPUTS: 允许主辅双输入切换。
- CONTROL_SERIAL_USART3/FEEDBACK_SERIAL_USART3: 右口串口控制/反馈。
- FLASH_WRITE_KEY: EEPROM 配置有效性版本键。
- TANK_STEERING: 坦克差速模式。
- ADC_ALTERNATE_CONNECT: 交换 ADC1/ADC2 接线解释。

### 16.12 Variant USART Key Parameters
### 16.12 VARIANT_USART 关键参数
- CONTROL_SERIAL_USART2/CONTROL_SERIAL_USART3: 串口控制输入选择。
- FEEDBACK_SERIAL_USART2/FEEDBACK_SERIAL_USART3: 串口反馈输出选择。
- SIDEBOARD_SERIAL_USART2/SIDEBOARD_SERIAL_USART3: sideboard 输入启用与优先级。
- DUAL_INPUTS: 串口与 sideboard 或双串口主辅切换。

### 16.13 Variant Nunchuk Key Parameters
### 16.13 VARIANT_NUNCHUK 关键参数
- CONTROL_NUNCHUK: Nunchuk 输入使能。
- FILTER/SPEED_COEFFICIENT/STEER_COEFFICIENT: Nunchuk 场景下建议滤波与比例。
- SUPPORT_BUTTONS: Nunchuk 按键支持开关。

### 16.14 Variant PPM And PWM Key Parameters
### 16.14 VARIANT_PPM 与 VARIANT_PWM 关键参数
- CONTROL_PPM_LEFT/CONTROL_PPM_RIGHT: PPM 输入口选择与优先级。
- PPM_NUM_CHANNELS: PPM 总通道数。
- CONTROL_PWM_LEFT/CONTROL_PWM_RIGHT: PWM 输入口选择与优先级。
- TANK_STEERING: 双通道分别控制左右轮。
- INVERT_R_DIRECTION/INVERT_L_DIRECTION: 电机方向反向。

### 16.15 Variant IBUS Key Parameters
### 16.15 VARIANT_IBUS 关键参数
- CONTROL_IBUS: iBUS 解码使能。
- IBUS_NUM_CHANNELS: iBUS 通道数量。
- IBUS_LENGTH/IBUS_COMMAND: iBUS 帧格式参数。
- CONTROL_SERIAL_USART3: iBUS 数据入口串口。

### 16.16 Variant HOVERCAR Key Parameters
### 16.16 VARIANT_HOVERCAR 关键参数
- CTRL_MOD_REQ: 默认模式改为 VLT_MODE。
- CONTROL_ADC: 踏板模拟量输入。
- SIDEBOARD_SERIAL_USART3/FEEDBACK_SERIAL_USART3: sideboard 传感/灯控。
- MULTI_MODE_DRIVE: 上电三种驾驶模式选择逻辑总开关。
- MULTI_MODE_DRIVE_M1_MAX/M2_MAX/M3_MAX: 三档最大目标命令。
- MULTI_MODE_DRIVE_M1_RATE/M2_RATE/M3_RATE: 三档响应速率。
- MULTI_MODE_M1_I_MOT_MAX/M2_I_MOT_MAX/M3_I_MOT_MAX: 三档电流上限。
- MULTI_MODE_M1_N_MOT_MAX/M2_N_MOT_MAX/M3_N_MOT_MAX: 三档速度上限。
- MULTIPLE_TAP_NR/MULTIPLE_TAP_HI/MULTIPLE_TAP_LO/MULTIPLE_TAP_TIMEOUT: 多击检测参数。

### 16.17 Variant HOVERBOARD Key Parameters
### 16.17 VARIANT_HOVERBOARD 关键参数
- SIDEBOARD_SERIAL_USART2/SIDEBOARD_SERIAL_USART3: 双 sideboard 通道定义。
- FEEDBACK_SERIAL_USART2/FEEDBACK_SERIAL_USART3: sideboard 回传配置。
- PRI_INPUT1/PRI_INPUT2/AUX_INPUT1/AUX_INPUT2: hoverboard 模式输入映射。

### 16.18 Variant TRANSPOTTER Key Parameters
### 16.18 VARIANT_TRANSPOTTER 关键参数
- CONTROL_GAMETRAK: Gametrak 输入逻辑使能。
- SUPPORT_LCD/SUPPORT_NUNCHUK: 外设支持开关。
- GAMETRAK_CONNECTION_NORMAL/GAMETRAK_CONNECTION_ALTERNATE: Gametrak 接线映射选择。
- ROT_P: 方向控制 P 系数。
- INVERT_R_DIRECTION/INVERT_L_DIRECTION: 电机方向反向。

### 16.19 Variant SKATEBOARD Key Parameters
### 16.19 VARIANT_SKATEBOARD 关键参数
- CTRL_MOD_REQ: 默认模式改为 TRQ_MODE。
- CONTROL_PWM_RIGHT 或 CONTROL_PWM_LEFT: 油门输入通道。
- INPUT_BRK: 松油门回生刹车强度。
- STEER_COEFFICIENT: 滑板场景常设为 0 关闭转向混控。

### 16.20 Practical Usage Notes
### 16.20 使用建议
When changing variant family, first switch VARIANT_xxx, then verify cable-conflict checks compile cleanly.
切换变体时先改 VARIANT_xxx，再确认线缆冲突检查可通过编译。
When changing FLASH_WRITE_KEY, previously saved EEPROM config will be ignored.
修改 FLASH_WRITE_KEY 后，旧 EEPROM 配置会被视为无效。
For current-related tuning, adjust I_MOT_MAX first, then I_DC_MAX with safety margin.
电流调参建议先调 I_MOT_MAX，再留安全余量配置 I_DC_MAX。
For responsiveness tuning, jointly adjust RATE, FILTER, SPEED_COEFFICIENT, and STEER_COEFFICIENT.
响应性调优应联合调整 RATE、FILTER、SPEED_COEFFICIENT、STEER_COEFFICIENT。

---

End of detailed design.
详细设计结束。
