/*
 * Ambient Frame —— 手势交互氛围画框
 * 主控: ESP32-WROOM-32
 * 光源: WS2812B RGB 灯带
 * 传感器: DHT11 (温度/湿度), 红外避障模块 (靠近检测)
 * 显示: 0.96" SPI SSD1306 OLED (128x64)
 *
 * 库依赖(全部为本地已安装库):
 *   FastLED / Adafruit_SSD1306(+GFX+BusIO) / DHT sensor library for ESPx (DHTesp)
 *
 * 功能:
 *   1. 红外避障模块控制开关灯 —— 靠近开 / 离开关 (默认), 也可切换为每次靠近翻转
 *   2. DHT11 温度实时映射为灯光颜色 —— 冷(蓝) -> 暖(红)
 *   3. OLED 显示温湿度与灯光状态
 */

#include <Arduino.h>
#include <FastLED.h>
#include <DHTesp.h>
#include <Adafruit_SSD1306.h>

// ===================== 引脚定义 =====================
#define PIN_LED   12    // WS2812B 数据线 DIN
#define PIN_DHT   27    // DHT11 数据线 DATA (S)
#define PIN_IR    5     // 红外避障模块 OUT (数字输出)

#define OLED_SCK  18    // SSD1306 SCK/CLK (DO)
#define OLED_MOSI 14    // SSD1306 MOSI/DATA (D1)
#define OLED_CS   26    // SSD1306 CS
#define OLED_DC   17    // SSD1306 DC (数据/命令切换)
#define OLED_RES  16    // SSD1306 RES/RST

// ===================== 灯带参数 =====================
#define NUM_LEDS        30    // 灯珠数量, 按实际修改
#define LED_BRIGHTNESS  200   // 最大亮度 (0-255)

// ===================== 交互模式 =====================
#define MODE_PRESENCE   0     // 靠近亮, 离开灭
#define MODE_TOGGLE     1     // 每次靠近翻转一次开关
#define INTERACT_MODE   MODE_PRESENCE

// 大多数红外避障模块输出"低电平"表示检测到障碍; 如果你的模块相反, 改成 HIGH
#define IR_ACTIVE_LEVEL LOW

#define DEBOUNCE_MS     80     // 红外信号去抖时间
#define OFF_DELAY_MS    1500   // 离开后延迟熄灭(Presence 模式), 防手在边界来回抖动
#define FADE_STEP       8      // 亮度渐变步进(数值越大越干脆, 越小越柔和)

// ===================== 温度 -> 颜色映射 =====================
#define TEMP_MIN  10.0f       // 低于此温度 => 纯冷色(蓝)
#define TEMP_MAX  40.0f       // 高于此温度 => 纯暖色(红)
#define HUE_COLD  160         // 蓝
#define HUE_HOT   0           // 红 (HUE 从 160 降到 0, 经过青->绿->黄->红)

CRGB  leds[NUM_LEDS];
DHTesp dht;
Adafruit_SSD1306 display(128, 64, OLED_MOSI, OLED_SCK, OLED_DC, OLED_RES, OLED_CS);

// ===================== 运行状态 =====================
bool        nearState      = false;   // 红外: 是否靠近(已去抖)
bool        rawNear        = false;   // 红外: 原始读数
unsigned long lastNearChange = 0;
unsigned long lastNearMs     = 0;     // 最后一次靠近的时刻
bool        lightOn        = false;   // 灯光目标状态
uint8_t     currentBright  = 0;       // 当前实际亮度

float       lastTemp = NAN, lastHumi = NAN;
unsigned long lastDhtRead   = 0;
unsigned long lastOledUpdate = 0;

void readIR();
void readDHT();
void updateLight();
void updateOled();

void setup() {
  Serial.begin(115200);
  Serial.println("Ambient Frame boot");

  FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  dht.setup(PIN_DHT, DHTesp::DHT11);
  pinMode(PIN_IR, INPUT);

  display.begin(SSD1306_SWITCHCAPVCC);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);
  display.println("Ambient Frame");
  display.setCursor(0, 32);
  display.println("starting...");
  display.display();
  delay(800);
}

void loop() {
  readIR();
  readDHT();
  updateLight();
  updateOled();
  FastLED.show();
  delay(20);
}

// 读取红外避障模块, 带去抖, 触发开关逻辑
void readIR() {
  bool raw = (digitalRead(PIN_IR) == IR_ACTIVE_LEVEL);

  if (raw != rawNear) {           // 记录电平变化的时刻, 用于去抖
    rawNear = raw;
    lastNearChange = millis();
  }

  if (millis() - lastNearChange >= DEBOUNCE_MS) {
    if (raw && !nearState) {      // 靠近(上升沿)
      nearState  = true;
      lastNearMs = millis();
#if INTERACT_MODE == MODE_TOGGLE
      lightOn = !lightOn;
#else
      lightOn = true;
#endif
      Serial.println("Near detected");
    } else if (!raw && nearState) {
      nearState = false;
      Serial.println("Away");
    }
  }
}

// 每 2 秒读取一次 DHT11(该传感器读取间隔不能小于 1~2s)
void readDHT() {
  if (millis() - lastDhtRead < 2000) return;
  lastDhtRead = millis();

  TempAndHumidity th = dht.getTempAndHumidity();
  if (dht.getStatus() == DHTesp::ERROR_NONE) {
    lastTemp = th.temperature;
    lastHumi = th.humidity;
  }
}

// 根据温度计算颜色, 根据灯光状态做亮度渐变, 填充灯带
void updateLight() {
  float t    = isnan(lastTemp) ? 24.0f : lastTemp;   // 没读到温度前默认 24 度
  float tc   = constrain(t, TEMP_MIN, TEMP_MAX);
  float ratio = (tc - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
  uint8_t hue = (uint8_t)(HUE_COLD + ratio * (HUE_HOT - HUE_COLD));

#if INTERACT_MODE == MODE_PRESENCE
  if (!nearState && (millis() - lastNearMs) > OFF_DELAY_MS) {
    lightOn = false;
  }
#endif

  uint8_t target = lightOn ? LED_BRIGHTNESS : 0;
  if (currentBright < target) {
    currentBright += FADE_STEP;
    if (currentBright > target) currentBright = target;
  } else if (currentBright > target) {
    currentBright = (currentBright > FADE_STEP) ? (currentBright - FADE_STEP) : 0;
  }

  fill_solid(leds, NUM_LEDS, CHSV(hue, 255, currentBright));
}

// 每秒刷新一次 OLED
void updateOled() {
  if (millis() - lastOledUpdate < 1000) return;
  lastOledUpdate = millis();

  display.clearDisplay();

  display.setCursor(0, 0);
  display.print("Temp: ");
  if (!isnan(lastTemp)) {
    display.print(lastTemp, 1);
    display.print(" C");
  } else {
    display.print("--");
  }

  display.setCursor(0, 12);
  display.print("Humi: ");
  if (!isnan(lastHumi)) {
    display.print(lastHumi, 1);
    display.print(" %");
  } else {
    display.print("--");
  }

  display.setCursor(0, 24);
  display.print("Light: ");
  display.print(lightOn ? "ON " : "OFF");
  if (nearState) display.print("(near)");

  display.setCursor(0, 36);
  display.print("Mode: ");
#if INTERACT_MODE == MODE_TOGGLE
  display.print("toggle");
#else
  display.print("presence");
#endif

  display.display();
}
