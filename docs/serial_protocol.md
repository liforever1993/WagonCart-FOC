# WagonCart-FOC 串口通信协议文档

> 版本：v1.0  
> 基于代码提交：`app/comms.c`、`app/comms.h`、`modules/util.c`、`app/main.c`  
> 适用固件变体：VARIANT_PWM（默认），兼容 VARIANT_ADC、VARIANT_USART 等  
> 硬件平台：STM32F103xC  
> 文档目的：供蓝牙模块（如 HC-05/HC-06/CH9141 等）透传对接时参考

---

## 0. 快速开始

蓝牙模块连接并进入 AT 透传后，用 App 立即试以下几条命令：

```
HELP\r\n             → 打印所有可用命令、参数、变量及帮助文本
GET\r\n              → 打印所有参数当前值/初始值/范围
GET I_MOT_MAX\r\n    → 单条参数详情
SET I_MOT_MAX 12\r\n → 将最大电流设为 12A（成功后返回 OK\r\n 并蜂鸣 5 短声）
WATCH SPD_AVG\r\n    → 开启转速自动推送（每 ~125ms 主控输出一次）
WATCH SPD_AVG\r\n    → 再发一次即取消
SAVE\r\n             → 将所有修改写入 Flash（OK\r\n 成功）
```

> ASCII 帧**必须**以 `\r\n` 或至少 `\n` 结尾；命令和参数名全部**大写**。

---

## 1. 概述

本固件的串口通信分为三层，均可通过同一 UART（USART2 或 USART3）传输，由编译宏控制启用：

| 协议层 | 类型 | 方向 | 说明 | 启用宏 |
|--------|------|------|------|--------|
| ASCII 调试协议 | 文本 | 双向 | 参数读写、命令执行、Watch 变量 | `DEBUG_SERIAL_USARTx` + `DEBUG_SERIAL_PROTOCOL` |
| 二进制控制协议 | 二进制 | 主机→从机 | 实时 steer/speed 指令 | `CONTROL_SERIAL_USARTx` |
| 二进制反馈协议 | 二进制 | 从机→主机 | 电机转速、电流、电压、温度状态 | `FEEDBACK_SERIAL_USARTx` |
| 侧板协议 | 二进制 | 双向 | 副板传感器/LED | `SIDEBOARD_SERIAL_USARTx` |

> **重要**：`DEBUG_SERIAL_USARTx` 与 `FEEDBACK_SERIAL_USARTx` 互斥；同一 USART 上 `DEBUG_SERIAL` 和 `CONTROL_SERIAL` 可共存。本项目默认配置 `VARIANT_PWM` 使用 USART2 作为 DEBUG 串口，USART3 被 PWM 占用。蓝牙透传建议接入 DEBUG 串口。

---

## 2. UART 物理层配置

| 项目 | USART2（左传感器板电缆） | USART3（右传感器板电缆） |
|------|--------------------------|--------------------------|
| TX 引脚 | PA2 | PB10 |
| RX 引脚 | PA3 | PB11 |
| 波特率 | 115200（默认 `USART2_BAUD`） | 115200（默认 `USART3_BAUD`） |
| 数据位 | 8 | 8 |
| 停止位 | 1 | 1 |
| 校验位 | None | None |
| 流控 | None | None |
| 电平 | 3.3V TTL（严禁接红线上的 15V！） | 3.3V TTL |
| 接收方式 | DMA Circular + IDLE 中断 | DMA Circular + IDLE 中断 |
| Rx Buffer | 64 字节 (`SERIAL_BUFFER_SIZE`) | 64 字节 |

---

## 3. ASCII 调试协议（核心）

### 3.1 帧格式

```
$<COMMAND>[ <PARAM>[ <VALUE>]][\r\n]
```

| 字段 | 说明 |
|------|------|
| `$` | 帧起始标志，固定 1 字节 |
| `COMMAND` | 命令字，见 §3.2，大小写敏感 |
| 空格 | 命令与参数之间用一个 ASCII 0x20 空格分隔 |
| `PARAM` | 参数名（仅 SET/GET/INIT/WATCH 携带），见 §4，大写 |
| 空格 | 参数与值之间用一个空格分隔 |
| `VALUE` | 十进制整数，可带负号（`-`），范围 -32768 ~ 32767 |
| `\r\n` | 帧终止，LF 必填，CR 可选（代码同时接受 `\n` 和 `\r` 作为终止） |

> 解析器使用 DMA + IDLE 空闲中断，遇到 `$` 开始收集，遇到 `\n`/`\r` 结束。**蓝牙模块必须将一帧完整打包成单次发送**，不要分包。

### 3.2 命令列表

| 命令 | 类型 | 功能 | 带参数 | 带值 | 示例 |
|------|------|------|--------|------|------|
| `GET` | READ | 获取所有参数的名称/值/初始化值/范围 | 可选 | 否 | `$GET\r\n` → 打印全部；`$GET I_MOT_MAX\r\n` → 打印单条 |
| `HELP` | READ | 打印命令/参数/变量的帮助文本 | 可选 | 否 | `$HELP\r\n` |
| `WATCH` | READ | 切换某变量的 Watch 开关（最多 15 个） | 必须 | 否 | `$WATCH SPD_AVG\r\n`；再次发送即取消 |
| `SET` | WRITE | 设置参数值（仅限 PARAMETER 类型，不可设 VARIABLE） | 必须 | 必须 | `$SET I_MOT_MAX 12\r\n` |
| `INIT` | WRITE | 将指定参数恢复为 EEPROM/Config.h 中的初始值 | 必须 | 否 | `$INIT CTRL_MOD\r\n` |
| `SAVE` | WRITE | 将所有已改动的参数保存到 EEPROM（Flash 写入） | 否 | 否 | `$SAVE\r\n` |

> **WATCH 机制**：开启 Watch 后，主控在主循环中每 125 ms（`main_loop_counter % 25` ≈ 25 × 5 ms）自动输出一次所有 Watch 变量的当前值（见 §3.4 输出帧）。

### 3.3 输入解析规则（handle_input）

```
┌──────────┐
│ '$' 起始 │──否──→ 丢弃
└────┬─────┘
     │是
     ▼
┌──────────────┐
│ 帧尾 \r/\n？ │──否──→ Err7（帧尾预期错误）
└────┬─────────┘
     │是
     ▼
┌──────────────┐
│ 查找命令字   │──否──→ Err1（命令未找到）
└────┬─────────┘
     │是
     ▼
┌──────────────────┐
│ 命令后是 \r/\n？ │──是──→ 无参命令（SAVE）
└────┬─────────────┘
     │否
     ▼
┌──────────────┐
│ 查找参数名   │──否──→ Err2（参数未找到）
└────┬─────────┘
     │是
     ▼
┌─────────────────────────────┐
│ WRITE 命令 + VARIABLE 类型？ │──是──→ Err3（VARIABLE 不可写）
└────┬────────────────────────┘
     │否
     ▼
┌───────────────────────────┐
│ 参数后是 \r/\n？          │──是──→ 单参命令（GET/WATCH/INIT）
└────┬──────────────────────┘
     │否
     ▼
┌──────────────────────┐
│ 读取可选 '-' + 数字  │──无数字──→ Err5（需要值）
└────┬─────────────────┘
     │有
     ▼
┌───────────────────────────┐
│ 值域检查                  │──超范围──→ Err4（值越界）
└────┬──────────────────────┘
     │通过
     ▼
   执行命令
```

### 3.4 输出帧格式

主控发送三类响应帧：

**(1) 参数定义帧** — `#` 开头（GET/SET/INIT 触发）
```
# name:"<NAME>" value:<VALUE> init:<INIT> min:<MIN> max:<MAX>\r\n
```
示例：
```
# name:"I_MOT_MAX" value:9 init:9 min:1 max:40\r\n
```

**(2) Watch 输出帧** — 周期性自动输出
```
<NAME1>:<VAL1> <NAME2>:<VAL2> ... \r\n
```
示例：
```
SPD_AVG:120 SPDL:118 SPDR:122 BATV:3972\r\n
```

**(3) 帮助帧** — `?` 开头（HELP 触发）
```
? Commands\r\n
? "GET":"Get Parameter/Variable"\r\n
? Parameters\r\n
? "CTRL_MOD":"Ctrl mode 1:VLT 2:SPD 3:TRQ" [min:1 max:3]\r\n
? Variables\r\n
? "SPD_AVG":"Motor Measured Avg RPM"\r\n
```

**(4) 错误帧** — `!` 开头
```
! Err<N>:"<描述>"\r\n
```
完整错误码见 §7。

**(5) 成功帧** — `OK\r\n`
SET / INIT / SAVE 命令执行成功后主控发送 `OK\r\n`。

---

## 4. 参数与变量表

`params[]` 数组定义于 `app/comms.c`，每个条目包含以下转换字段：

| 字段 | 含义 |
|------|------|
| `div` | 外部→内部分子，内部→外部分母 |
| `mul` | 内部→外部分子，外部→内部分母 |
| `fix` | 内部→外部右移位数（2 的幂缩放），外部→内部左移 |

**转换公式**：
```
intToExt:  ext = (int * mul) / div >> fix
extToInt:  int = (ext * div) << fix / mul
```
> 注意：`fix` 是最后一步位移；`A2BIT_CONV = 50`（电流转换系数：1A = 50 ADC bits）。

### 4.1 CONTROL PARAMETERS（可 SET / INIT / GET）

| 索引 | 名称 | 内部类型 | 外部单位 | 范围 Min | 范围 Max | 默认值 | div | mul | fix | 说明 |
|------|------|----------|----------|----------|----------|--------|-----|-----|-----|------|
| 0 | `CTRL_MOD` | UINT8_T | enum | 1 | 3 | `TRQ_MODE`(3) | 0 | 0 | 0 | 控制模式：1=VLT，2=SPD，3=TRQ |
| 1 | `CTRL_TYP` | UINT8_T | enum | 0 | 2 | `FOC_CTRL`(2) | 0 | 0 | 0 | 控制类型：0=COM，1=SIN，2=FOC |
| 2 | `I_MOT_MAX` | UINT16_T | A | 1 | 40 | `I_MOT_MAX`(9) | 50 | 0 | 4 | 单电机最大电流 |
| 3 | `N_MOT_MAX` | UINT16_T | rpm | 10 | 2000 | `N_MOT_MAX`(160) | 0 | 0 | 4 | 电机最大转速 |
| 4 | `FI_WEAK_ENA` | UINT8_T | 开关 | 0 | 1 | 0 | 0 | 0 | 0 | 弱磁使能 |
| 5 | `FI_WEAK_HI` | UINT16_T | (输入目标) | 0 | 1500 | `FIELD_WEAK_HI`(1000) | 0 | 0 | 4 | 弱磁高阈值 |
| 6 | `FI_WEAK_LO` | UINT16_T | (输入目标) | 0 | 1000 | `FIELD_WEAK_LO`(750) | 0 | 0 | 4 | 弱磁低阈值 |
| 7 | `FI_WEAK_MAX` | UINT16_T | A(FOC) | 0 | 20 | `FIELD_WEAK_MAX`(5) | 50 | 0 | 4 | 弱磁 D 轴电流上限 |
| 8 | `PHA_ADV_MAX` | UINT16_T | deg(SIN) | 0 | 55 | `PHASE_ADV_MAX`(25) | 0 | 0 | 4 | 最大相位超前角 |

### 4.2 INPUT PARAMETERS（可 SET / INIT / GET）

| 索引 | 名称 | 内部类型 | 外部单位 | 范围 Min | 范围 Max | 默认值 | 说明 |
|------|------|----------|----------|----------|----------|--------|------|
| 9 | `IN1_TYP` | UINT8_T | enum | 0 | 3 | 0 | Input1 类型：0=禁用, 1=常规, 2=中置, 3=自动检测 |
| 10 | `IN1_MIN` | INT16_T | raw | `RAW_MIN` | `RAW_MAX` | `RAW_MIN` | Input1 最小值（ADC 为 0/4095，非 ADC 为 -1000/1000） |
| 11 | `IN1_MID` | INT16_T | raw | `RAW_MIN` | `RAW_MAX` | 0 | Input1 中值 |
| 12 | `IN1_MAX` | INT16_T | raw | `RAW_MIN` | `RAW_MAX` | `RAW_MAX` | Input1 最大值 |
| 13 | `IN2_TYP` | UINT8_T | enum | 0 | 3 | 0 | Input2 类型 |
| 14 | `IN2_MIN` | INT16_T | raw | `RAW_MIN` | `RAW_MAX` | `RAW_MIN` | Input2 最小值 |
| 15 | `IN2_MID` | INT16_T | raw | `RAW_MIN` | `RAW_MAX` | 0 | Input2 中值 |
| 16 | `IN2_MAX` | INT16_T | raw | `RAW_MIN` | `RAW_MAX` | `RAW_MAX` | Input2 最大值 |

> **RAW_MIN/RAW_MAX**：`CONTROL_ADC` 模式下为 0 / 4095；其他模式为 -1000 / 1000。  
> 启用双输入（`DUAL_INPUTS`）后还会出现 `AUX_IN1_TYP/MIN/MID/MAX`、`AUX_IN2_*` 等 8 个参数。

### 4.3 VARIABLES（只读，仅 GET / WATCH）

| 索引 | 名称 | 内部类型 | 单位 | 说明 |
|------|------|----------|------|------|
| 17 | `DC_CURR` | INT16_T | A ×100 | 总直流母线电流 |
| 18 | `RDC_CURR` | INT16_T | A ×100 | 右电机直流电流 |
| 19 | `LDC_CURR` | INT16_T | A ×100 | 左电机直流电流 |
| 20 | `CMDL` | INT16_T | cmd | 左电机指令 [-1000, 1000] |
| 21 | `CMDR` | INT16_T | cmd | 右电机指令 [-1000, 1000] |
| 22 | `SPD_AVG` | INT16_T | rpm | 平均电机转速（绝对值） |
| 23 | `SPDL` | INT16_T | rpm | 左电机实测转速 |
| 24 | `SPDR` | INT16_T | rpm | 右电机实测转速 |
| 25 | `RATE` | — | ×10 | 加速率（默认 64 = 4.0） |
| 26 | `SPD_COEF` | — | ×10 | 速度系数（默认 16384 = 1.0 fixdt，外部换算 ×10 后显示） |
| 27 | `STR_COEF` | — | ×10 | 转向系数（默认 16384 = 1.0） |
| 28 | `BATV` | INT16_T | V ×100 | 校准后电池电压 |
| 29 | `TEMP` | INT16_T | °C ×10 | 校准后板载温度 |
| — | `IN1_RAW` | INT16_T | raw | Input1 原始值（只读） |
| — | `IN1_CMD` | INT16_T | cmd | Input1 命令值（只读） |
| — | `IN2_RAW` | INT16_T | raw | Input2 原始值（只读） |
| — | `IN2_CMD` | INT16_T | cmd | Input2 命令值（只读） |

> `RATE`/`SPD_COEF`/`STR_COEF` 三条在 `params[]` 中 `datatype=0`、`div=0, mul=10, fix=14`，值来源于 config.h 的 `fixdt` 宏。

### 4.4 值转换举例

以 `I_MOT_MAX` 为例（`div=50, mul=0, fix=4`）：
- 外部设置 `$SET I_MOT_MAX 12\r\n`（12A）
- `extToInt`: `12 * 50 << 4 = 9600`（内部 fixdt(1,16,4) 格式）
- `GET I_MOT_MAX` 返回 `value:12`
- 实际写入 `rtP_Left.i_max = 9600`

以 `N_MOT_MAX` 为例（`div=0, mul=0, fix=4`）：
- 外部设置 160 rpm
- 内部值 = `160 << 4 = 2560`

---

## 5. 二进制控制协议（CONTROL_SERIAL）

> 启用宏：`CONTROL_SERIAL_USART2` 或 `CONTROL_SERIAL_USART3`  
> 此协议用于蓝牙模块直接下发转向/速度指令，实现遥控驾驶。

### 5.1 帧结构（小端序，总 10 字节）

```c
typedef struct {
    uint16_t start;     // 固定 0xABCD (SERIAL_START_FRAME)
    int16_t  steer;     // 转向指令，范围 [-1000, 1000]
    int16_t  speed;     // 速度指令，范围 [-1000, 1000]
    uint16_t checksum;  // XOR 校验
} SerialCommand;
```

| 偏移 | 长度 | 字段 | 说明 |
|------|------|------|------|
| 0 | 2 | start | `0xABCD`（低字节 `0xCD` 在前） |
| 2 | 2 | steer | 转向值，`int16_t` 小端 |
| 4 | 2 | speed | 速度值，`int16_t` 小端 |
| 6 | 2 | checksum | `start ^ steer ^ speed` |

### 5.2 校验算法

```c
checksum = (uint16_t)(start ^ steer ^ speed);
```

### 5.3 示例（steer=0, speed=500）

```
CD AB 00 00 F4 01 09 04
│  │  │  │  │  │  │  └─ checksum低字节
│  │  │  │  │  │  └──── checksum高字节 = 0x04
│  │  │  │  │  └─────── speed高字节   = 0x01
│  │  │  │  └────────── speed低字节   = 0xF4 (500)
│  │  │  └───────────── steer高字节   = 0x00
│  │  └──────────────── steer低字节   = 0x00
│  └──────────────────── start高字节  = 0xAB
└─────────────────────── start低字节  = 0xCD
checksum = 0xABCD ^ 0x0000 ^ 0x01F4 = 0x0409 → 传输为 09 04
```

> **超时保护**：连续 `SERIAL_TIMEOUT`（默认 160 次主循环 ≈ 0.8s）未收到合法帧，主控会将输入归零并进入 OPEN_MODE。蓝牙模块必须以 ≤ 20Hz 的频率持续发送。

---

## 6. 二进制反馈协议（FEEDBACK_SERIAL）

> 启用宏：`FEEDBACK_SERIAL_USART2` / `FEEDBACK_SERIAL_USART3`  
> 主控以 10ms 周期（`main_loop_counter % 2`）通过 DMA 自动发送。

### 6.1 帧结构（小端序，总 20 字节）

```c
typedef struct {
    uint16_t start;         // 0xABCD
    int16_t  cmd1;          // input1 命令 [-1000,1000]
    int16_t  cmd2;          // input2 命令 [-1000,1000]
    int16_t  speedR_meas;   // 右电机实测转速 rpm
    int16_t  speedL_meas;   // 左电机实测转速 rpm
    int16_t  batVoltage;    // 校准后电池电压 ×100
    int16_t  boardTemp;     // 校准后温度 °C ×10
    uint16_t cmdLed;        // 侧板 LED 状态
    uint16_t checksum;      // XOR 全部字段
} SerialFeedback;
```

### 6.2 校验算法

```c
checksum = start ^ cmd1 ^ cmd2 ^ speedR_meas ^ speedL_meas
         ^ batVoltage ^ boardTemp ^ cmdLed;
```

### 6.3 读取流程

1. 每 10ms 接收一帧，共 20 字节
2. 校验 `start == 0xABCD`
3. 计算 checksum 并比对
4. 解析字段供 App 显示/记录

---

## 7. 错误码

主控返回的 `! ErrN:"描述"\r\n` 帧：

| 错误码 | 描述 | 触发场景 |
|--------|------|----------|
| Err1 | Command not found | `$XXX` 中 XXX 不是有效命令 |
| Err2 | Parameter not found | 命令后没有找到有效参数名 |
| Err3 | This command cannot be used with a Variable | 对只读 VARIABLE 执行 SET/INIT |
| Err4 | Value not in range | 值超出参数 min/max |
| Err5 | Value expected | SET 命令未提供 VALUE |
| Err6 | Start of line expected | 首字符非 `$`（内部保留，不对外） |
| Err7 | End of line expected | 帧未正确以 `\r`/`\n` 结束 |
| Err8 | Parameter expected | 命令需要参数但未提供（如 `$WATCH\r\n`） |
| Err9 | Uncaught error | 其他未分类错误 |
| Err10 | Watch list is full | Watch 数量已达 15 个上限 |

---

## 8. 蓝牙透传对接指南

### 8.1 推荐硬件与接线

| 主控侧 | 蓝牙模块 | 说明 |
|--------|----------|------|
| PA2 (USART2_TX) | RX | 电平相同，可直连 |
| PA3 (USART2_RX) | TX | 可直连 |
| GND | GND | 必须共地 |
| 3.3V | VCC | 勿接 15V；若模块 5V 供电需确认 IO 电平 |

> PWM 变体默认 USART3 被占用，建议接 USART2（`DEBUG_SERIAL_USART2`），并在 `config.h` 中取消注释：
> ```c
> #define DEBUG_SERIAL_USART2
> #define DEBUG_SERIAL_PROTOCOL
> ```
> **注意**：`VARIANT_PWM` 已自动启用 `DEBUG_SERIAL_USART2`，只需额外 `#define DEBUG_SERIAL_PROTOCOL`。

### 8.2 蓝牙 AT 指令参考（HC-05 示例）

上电（未连接状态）通过串口发送：

```
AT\r\n            → 测试通信
AT+NAME=WagonCart → 修改蓝牙名
AT+BAUD=6         → 115200 波特率（6:115200, 4:9600, 8:460800）
AT+ORGL           → 恢复出厂
```

### 8.3 App 侧建议实现

```
初始化序列：
  1. 连接蓝牙
  2. 发送 $HELP\r\n  获取帮助（可选）
  3. 发送 $GET\r\n   下载全部参数做 UI
  4. 发送 $SAVE\r\n  确认 EEPROM 就绪（可选）

运行时：
  - 周期发送 $WATCH SPD_AVG\r\n $WATCH BATV\r\n ... 
    让主控自动推送状态（每 125ms 一帧）
  - 参数调整：$SET I_MOT_MAX 12\r\n → 收到 OK\r\n
  - 参数保存：$SAVE\r\n → 收到 OK\r\n

需要实时控制（遥控）：
  - 启用 CONTROL_SERIAL_USART2 + FEEDBACK_SERIAL_USART2
  - 按二进制协议每 20-50ms 发送一帧 SerialCommand
  - 接收主控每 10ms 的 SerialFeedback 状态
```

### 8.4 两种控制方案对比

| 维度 | ASCII + SET | 二进制 CONTROL_SERIAL |
|------|-------------|----------------------|
| 带宽 | 低（文本） | 低（10B/帧） |
| 实时性 | Watch 125ms 延迟 | 10ms 反馈周期 |
| 可调参数 | 全部 | 仅 steer/speed |
| 校验 | 隐式（人类可读） | XOR 显式 |
| 实现难度 | 低 | 中（需小端序 + 结构体） |
| 推荐场景 | 调试、参数配置、低速遥控 | 游戏杆/遥控器实时驾驶 |

### 8.5 注意事项

1. **帧结束符**：ASCII 协议必须以 `\r\n` 或至少 `\n` 结尾，否则 Err7。
2. **大小写敏感**：命令和参数名均为大写（如 `$SET`，不是 `$set`）。
3. **Watch 上限**：最多 15 个，超出返回 Err10。
4. **参数修改确认**：主控会蜂鸣 5 短声提示值被更改（`beepShort(5)`）。
5. **Flash 写入**：`$SAVE` 会写入内部 Flash，**频繁写入会减少寿命**（典型 10000 次）。
6. **二进制协议禁用**：若同时启用 `DEBUG_SERIAL_USARTx` 与 `FEEDBACK_SERIAL_USARTx`，编译会报错，只能二选一。
7. **DMA 半满问题**：接收缓冲区 64 字节，单行 ASCII 帧建议 ≤ 60 字节。
8. **Watchdog/超时**：二进制协议超时约 0.8s 会使主控进入 OPEN_MODE 保护，蓝牙断开会自动安全停车。

---

## 9. 数据流时序图

```
 App / 蓝牙              STM32 主控
   │                        │
   │── $SET I_MOT_MAX 12\r\n →  │
   │                        │── Err4 范围校验 → 范围检查 → 通过
   │ ← OK\r\n ─────────────│── beepShort(5) 修改提示
   │                        │
   │── $WATCH SPD_AVG\r.n ──→  │── watchParamVal 添加到列表
   │ ← OK\r\n ─────────────│
   │                        │
   │                        │  (主循环每 125ms)
   │ ← SPD_AVG:120 BATV:3972\r\n │── printParamVal 自动输出
   │ ← SPD_AVG:118 BATV:3971\r\n │
   │                        │
   │── $GET I_MOT_MAX\r\n ──→  │── printParamDef
   │ ← # name:"I_MOT_MAX" value:12 init:9 min:1 max:40\r\n │
   │ ← OK\r\n ─────────────│
```

---

## 10. 代码引用

| 模块 | 文件 | 关键符号 |
|------|------|----------|
| 命令解析 | [comms.c](file:///Users/lihuaming/workspace/WagonCart-FOC/app/comms.c) | `handle_input`, `findCommand`, `findParam`, `params[]`, `commands[]`, `printError` |
| 参数转换 | [comms.c](file:///Users/lihuaming/workspace/WagonCart-FOC/app/comms.c#L389-L408) | `intToExt`, `extToInt`, `setParamValExt`, `getParamValExt` |
| UART 初始化 | [setup.c](file:///Users/lihuaming/workspace/WagonCart-FOC/platform/board/src/setup.c) | `UART2_Init`, `UART3_Init`, `HAL_UART_MspInit` |
| DMA + IDLE | [stm32f1xx_it.c](file:///Users/lihuaming/workspace/WagonCart-FOC/platform/board/src/stm32f1xx_it.c) | `USART2_IRQHandler`, `USART3_IRQHandler` |
| DMA 接收解析 | [util.c](file:///Users/lihuaming/workspace/WagonCart-FOC/modules/util.c) | `usart2_rx_check`, `usart3_rx_check`, `usart_process_debug`, `usart_process_command` |
| 二进制结构 | [util.h](file:///Users/lihuaming/workspace/WagonCart-FOC/modules/util.h) | `SerialCommand`, `SerialSideboard`, `InputStruct` |
| 反馈协议 | [main.c](file:///Users/lihuaming/workspace/WagonCart-FOC/app/main.c) | `SerialFeedback`, `HAL_UART_Transmit_DMA` |
| 编译配置 | [config.h](file:///Users/lihuaming/workspace/WagonCart-FOC/platform/config/config.h) | `DEBUG_SERIAL_USARTx`, `DEBUG_SERIAL_PROTOCOL`, `CONTROL_SERIAL_USARTx`, `FEEDBACK_SERIAL_USARTx`, `USARTx_BAUD` |
| 主循环入口 | [main.c](file:///Users/lihuaming/workspace/WagonCart-FOC/app/main.c#L494-L558) | `process_debug`, Watch 输出周期 `main_loop_counter % 25` |

---

## 11. 已知代码问题（对接时注意）

以下问题**已于 2026-10-24 修复**。记录在此便于理解代码演进：

### 11.1 ~~`errors[]` 数组缺逗号 — Err10 无法正常触发~~ ✅ 已修复

[`app/comms.c` 第 148-159 行](file:///Users/lihuaming/workspace/WagonCart-FOC/app/comms.c#L148-L159)：

修复前：
```c
const char *errors[9] = {
  ...
  "Uncaught error"    // 缺逗号
  "Watch list is full"
};
```

修复后：
```c
const char *errors[10] = {
  ...
  "Uncaught error",   // 补上逗号
  "Watch list is full"
};
```

### 11.2 ~~`parameter_entry_struct` 缺失 `addr` 字段~~ ✅ 已修复

[`app/comms.h` 第 93-109 行](file:///Users/lihuaming/workspace/WagonCart-FOC/app/comms.h#L93-L109)：

在结构体**末尾**追加 `const uint8_t addr;`。放在末尾而非中间，是因为 `params[]` 数组使用 C89 风格的位置初始化，中间插入字段会导致所有已有 initializer 的值静默错位。追加在末尾后，所有现有条目的 `addr` 自动零初始化，`saveAllParamVal()` / `getParamInitInt()` 中 `if (params[i].addr)` 检查保持 `false`，行为与修复前一致。

### 11.3 参数索引在双输入模式下会偏移

`DUAL_INPUTS` 宏启用时，`params[]` 中会在 `IN2_*` 后插入 8 条 `AUX_IN1_*` 和 `AUX_IN2_*` 条目（见 [`comms.c` 第 114-129 行](file:///Users/lihuaming/workspace/WagonCart-FOC/app/comms.c#L114-L129)）。App 侧**不要硬编码索引**，应动态解析 `$GET\r\n` 或 `$HELP\r\n` 的输出来建立参数名到索引的映射。

