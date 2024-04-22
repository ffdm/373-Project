#include <TJpg_Decoder.h>
#include "SPI.h"
#include <TFT_eSPI.h>
#include "Free_Fonts.h"

// LCD parameters
TFT_eSPI tft = TFT_eSPI();
// #define CS_PIN 15
// #define DC_PIN 2
// #define LED_PIN 16
// #define RESET_PIN 16
// #define MISO_PIN 12
// #define MOSI_PIN 13
// #define SCK_PIN 14

bool img = false;
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  //Serial.printf("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

  // rgb 565 uses 16 bits for each pixel, bitmap is 128 pixels
  // scaled_bitmap should be 512 pixels
  tft.pushImage(x, y, w, h, bitmap);

  Serial.println("bruh");
  // Return 1 to decode next block
  return 1;
}

void countdown() {
  tft.fillScreen(TFT_RED);
  tft.setFreeFont(FF48);
  tft.setCursor(155, 130);
  tft.print("3");
  delay(1000);

  tft.fillScreen(TFT_RED);
  tft.setCursor(155, 130);
  tft.print("2");
  delay(1000);

  tft.fillScreen(TFT_RED);
  tft.setCursor(155, 130);
  tft.print("1");
  delay(1000);

  tft.fillScreen(TFT_GREEN);
  tft.setCursor(125, 130);
  tft.print("GO!");
  delay(1000);
}

void setup() {
  // Initialize serial port
  Serial.begin(115200);

  // Initialize LCD
  tft.begin();
  tft.setTextColor(0xFFFF, 0x0000);
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setCallback(tft_output);
}

void loop() {
  countdown();
}