/*
 * LedBlackTest —— 灯带「纯黑」诊断程序
 *
 * 用途: 判断 off 时灯带跳动是硬件问题还是主程序问题。
 * 行为: 开机闪一下白光(确认灯带能正常收到数据), 之后只持续发送全黑帧, 不做任何其他事。
 *
 * 结果判断:
 *   - 纯黑状态下灯带仍跳动 => 100% 硬件/接线问题 (共地 / 3.3V 电平 / 数据线)
 *   - 纯黑状态下完全不跳 => 回主程序排查
 *
 * 引脚与灯珠数需和主程序一致。
 */

#include <Arduino.h>
#include <FastLED.h>

#define PIN_LED   12    // WS2812B DIN, 和主程序一致
#define NUM_LEDS  30    // 灯珠数量, 和主程序一致

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  Serial.println("LedBlackTest: boot flash (white) then pure black");

  FastLED.addLeds<WS2812B, PIN_LED, GRB>(leds, NUM_LEDS);

  // 开机闪一下白, 确认灯带/引脚/颜色顺序都能正常收到数据
  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  delay(400);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  Serial.println("LedBlackTest: now sending black only, watch the strip");
}

void loop() {
  // 持续发全黑, 每 20ms 一帧(与主程序一致)
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  static unsigned long last = 0;
  if (millis() - last >= 5000) {
    last = millis();
    Serial.println("still sending black...");
  }
  delay(20);
}
