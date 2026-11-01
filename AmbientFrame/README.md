# Ambient Frame —— 氛围画框固件

温度决定颜色、靠近决定开关的氛围灯。ESP32 + WS2812B + DHT11 + 红外避障 + SPI OLED。

## 接线表

| ESP32 引脚 | 元件 |
|-----------|------|
| GPIO12    | WS2812B DIN（数据线） |
| GPIO27    | DHT11 DATA（S） |
| GPIO5     | 红外避障模块 OUT |
| GPIO18    | OLED SCK/CLK（DO） |
| GPIO14    | OLED MOSI/DATA（D1） |
| GPIO26    | OLED CS |
| GPIO17    | OLED DC（数据/命令切换） |
| GPIO16    | OLED RES/RST |
| 3V3       | OLED VCC、避障模块 VCC、DHT11 VCC(+) |
| 5V        | 灯带 +5V，直连 5V2A 电源（勿从 ESP32 5V 引脚抽大电流） |
| GND       | **所有模块共地**（这是灯带不听话最常见的原因） |

> 灯带超过 10 颗建议：灯带 5V 直接接 5V2A 电源，ESP32 与灯带共地，数据线串 330Ω 电阻。
> 若颜色不对（红绿反了），把 `addLeds<WS2812B, PIN_LED, GRB>` 的 `GRB` 改成 `RGB`。
> GPIO12 是 strapping 脚（上电时不能为高，否则 ESP32 无法正常启动）。WS2812B 的 DIN 是高阻输入、不会把它拉高，正常接没问题；但别在 GPIO12 上接会「上电输出高」的东西。

## 库依赖（均已在你电脑本地安装，直接烧录即可）

- **FastLED**（灯带）
- **Adafruit SSD1306** + **Adafruit GFX Library** + **Adafruit BusIO**（OLED）
- **DHT sensor library for ESPx**（DHT11）

> 说明：本机 GitHub 连接被阻断，PlatformIO 无法下载 ESP32 内核依赖库，因此改用 **Arduino IDE / arduino-cli** 烧录（ESP32 内核 3.3.10 已装好）。
> 烧录命令：`arduino-cli compile --fqbn "esp32:esp32:esp32:UploadSpeed=115200" AmbientFrame/AmbientFrame.ino`，再 `arduino-cli upload -p COM4 --fqbn "esp32:esp32:esp32:UploadSpeed=115200" AmbientFrame/AmbientFrame.ino`（COM 号以设备管理器为准，本机是 CH340 的 COM4）。
> **必须带 `UploadSpeed=115200`**：默认 921600 在这块 CH340 上写 flash 会失败（`Unable to verify flash chip connection`）。

## 关键可调参数（在 `.ino` 顶部）

| 宏 | 说明 |
|----|------|
| `NUM_LEDS` | 灯珠数量 |
| `LED_BRIGHTNESS` | 最大亮度 0-255 |
| `INTERACT_MODE` | `MODE_PRESENCE`（靠近亮离开灭，默认）/ `MODE_TOGGLE`（每次靠近翻转） |
| `IR_ACTIVE_LEVEL` | 红外模块有效电平，默认 `LOW`，反了就改 `HIGH` |
| `OFF_DELAY_MS` | 离开后延迟熄灭时长（防手在边界抖动） |
| `FADE_STEP` | 亮度渐变速度 |
| `TEMP_MIN` / `TEMP_MAX` | 温度映射下限/上限（蓝 → 红） |

## 首次通电必做

1. **红外模块调距离**：模块上有电位器，拧到「手靠近 5-10cm 触发、离远不触发」的临界位置。
2. 打开串口监视器（115200），靠近/离开手，确认打印 `Near detected` / `Away`。若反了，把 `IR_ACTIVE_LEVEL` 改成 `HIGH`。
3. OLED 应显示 Temp / Humi / Light / Mode。若黑屏，检查 SPI 四根线 + RES 是否接对。

## 故障排查：off 时灯带跳动（鬼影/闪烁）

主程序在 off 时也在每 20ms 持续发送全黑帧，所以跳动基本是**数据线信号完整性问题**，按概率排查：

1. **共地（最常见）**：灯带电源的 GND 必须和 ESP32 的 GND 用短粗线直连。灯带单独 5V2A 供电时尤其容易漏接。
2. **3.3V 电平临界**：ESP32 高电平 3.3V，而 5V 供电的 WS2812B 阈值约 3.5V。修法二选一：
   - 第一颗灯珠 VCC 串一个 1N4148 / 1N4007 二极管（压降约 0.6V），把第一颗阈值降到 ≈3.1V；
   - 或加 74AHCT125 电平转换（3.3V → 5V，最稳）。
3. **数据线**：DIN 串 330–470Ω 电阻（靠近第一颗灯珠），线尽量短、别和电源线捆一起。
4. **去耦**：灯带 5V/GND 输入端并一个 100–1000µF 电解电容。

**定位方法**：烧 `LedBlackTest/LedBlackTest.ino`（只发全黑、开机闪一下白）。
- 纯黑仍跳动 → 100% 硬件/接线问题；
- 纯黑不跳动 → 回主程序排查。

## 说明

- DHT11 读取间隔至少 2 秒，固件已内置。
- OLED 用 Adafruit SSD1306 默认 5x7 字体、英文显示。想要中文需换带中文字库的库（如 U8g2 的 CJK 字体，占用较大 Flash）。
