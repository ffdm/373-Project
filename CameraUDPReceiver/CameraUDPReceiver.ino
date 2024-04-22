#include <WiFi.h>
#include <WiFiUdp.h>
#include <TJpg_Decoder.h>
#include "SPI.h"
#include <TFT_eSPI.h>

// # define DEBUG

// Which camera/receiver pair is this?
const int id = 1; // 1 or 2

// WiFi parameters
int status = WL_IDLE_STATUS;
const char* ssid = "Banana Phone";
const char* password = "373 Project";
unsigned int localPorts[] = {0, 3000, 4000};
WiFiUDP Udp;

// Image parameters
unsigned long start_time; // time in microseconds
uint8_t last_frame_num; // previous frame
uint8_t jpg_buffer[8000]; // store jpeg data received from camera
uint8_t temp_buffer[2000]; // store the current segment
uint8_t received_segs[4];

// LCD parameters
TFT_eSPI tft = TFT_eSPI();
// #define CS_PIN 15
// #define DC_PIN 2
// #define LED_PIN 16
// #define RESET_PIN 16
// #define MISO_PIN 12
// #define MOSI_PIN 13
// #define SCK_PIN 14

uint16_t scaled_bitmap_left[256]; // upper left and lower left blocks
uint16_t scaled_bitmap_right[256]; // upper left and lower left blocks

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  //Serial.printf("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);

  // rgb 565 uses 16 bits for each pixel, bitmap is 128 pixels
  // scaled_bitmap should be 512 pixels

  for(int i = 0; i < 128; i++) {
    int row = i / 16; // row in bitmap
    int col = i % 16; // col in bitmap

    if(col < 8) { // left
        int scaled_row = 2 * row; // row in scaled bitmap
        int scaled_col = 2 * col; // col in scaled bitmap
        int start = 16 * scaled_row + scaled_col; // index of upper left pixel
        scaled_bitmap_left[start] = bitmap[i];
        scaled_bitmap_left[start + 1] = bitmap[i];
        scaled_bitmap_left[start + 16] = bitmap[i];
        scaled_bitmap_left[start + 17] = bitmap[i];
    } else { // right
        int scaled_row = 2 * row; // row in scaled bitmap
        int scaled_col = 2 * (col - 8); // col in scaled bitmap
        int start = 16 * scaled_row + scaled_col; // index of upper left pixel
        scaled_bitmap_right[start] = bitmap[i];
        scaled_bitmap_right[start + 1] = bitmap[i];
        scaled_bitmap_right[start + 16] = bitmap[i];
        scaled_bitmap_right[start + 17] = bitmap[i];
    }
  }

  // to have 4 equivalent images in each corner (just for fun)
  // tft.pushImage(x, y, w, h, bitmap); // Upper left quadrant
  // tft.pushImage(x + 160, y, w, h, bitmap); // Upper right quadrant
  // tft.pushImage(x, y + 120, w, h, bitmap); // Lower left quadrant
  // tft.pushImage(x + 160, y + 120, w, h, bitmap); // Lower right quadrant

  // to scale 160 x 120 -> 320 x 240
  tft.pushImage(2*x, 2*y, w, h, scaled_bitmap_left); // Upper left quadrant
  tft.pushImage(2*x + w, 2*y, w, h, scaled_bitmap_right); // Upper right quadrant
  tft.pushImage(2*x, 2*y + h, w, h, scaled_bitmap_left + 128); // Lower left quadrant
  tft.pushImage(2*x + w, 2*y + h, w, h, scaled_bitmap_right + 128); // Lower right quadrant
  // Return 1 to decode next block
  return 1;
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

  // Connect to WiFi
  unsigned int localPort = localPorts[id];
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Attempting to connect");
  }
  Serial.println("Connected to wifi");
  printWifiStatus();
  Udp.begin(localPort);

  Serial.printf("Receiver %d listening on port %u\n", id, localPort);
}

void loop() {
  size_t buffer_length;
  uint8_t frame_num, segment_num, num_segments;
  
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();

  if (packetSize) {
    // 1. Read all data as fast as possible
    // Maybe do all of these as one read???
    Udp.read((uint8_t*) &buffer_length, 4);
    Udp.read((uint8_t*) &frame_num, 1);
    Udp.read((uint8_t*) &segment_num, 1);
    Udp.read((uint8_t*) &num_segments, 1);
    unsigned int bytes_read = Udp.available(); // PERF: maybe do not call this function and use a fixed size?
    Udp.read(temp_buffer, bytes_read); // PERF: maybe read into jpg_buffer directly?

    uint8_t last_segment = num_segments - 1;

    // Memcpy data into correct location in jpg_buffer (from temp_buffer)
    uint8_t* addr = jpg_buffer + (segment_num * 1430);
    memcpy(addr, temp_buffer, 1430);

    // 2. Print information about packet received
    // Example: Received packet: 1460 bytes, Frame: 100, Segment Number: 1 / 3
    #ifdef DEBUG
      Serial.printf("Received packet: %d bytes, Frame: %d, Segment Number: %d / %d\n", packetSize, frame_num, segment_num, last_segment);
    #endif

    // Error-checking for missed packets
    if(frame_num != last_frame_num) { // Different frame: clear received_segs
      for(int i = 0; i < 4; i++) {
        received_segs[i] = 0;
      }
    }
    last_frame_num = frame_num;

    received_segs[segment_num] = 1;

    if(segment_num == last_segment) {
      // Check that we received all segments
      uint8_t segs = 0;
      for(int i = 0 ; i < num_segments; i++) {
        segs += received_segs[i];
      }

      // Received all segments
      if(segs == num_segments) {
        #ifdef DEBUG
          Serial.println("Begin Draw");
        #endif
        unsigned long start_draw = millis();
        TJpgDec.drawJpg(0, 0, jpg_buffer, buffer_length);
        unsigned long end_draw = millis();
        Serial.printf("Time to draw: %d milliseconds\n", (end_draw - start_draw));
        #ifdef DEBUG
          Serial.println("Successfully drew pic");
        #endif

        // Calculate how long it took to receive the frame
        #ifdef DEBUG
          unsigned long elapsed_time = micros() - start_time;
          float frame_rate = (float) 1000000 / (float) elapsed_time;
          Serial.printf("Elapsed time: %4f microseconds, Frame rate: %4f\n", elapsed_time, frame_rate);
          start_time = micros();
        #endif
      }
    }
  }
}

void printWifiStatus() {

  // print the SSID of the network you're attached to:

  Serial.print("SSID: ");

  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:

  IPAddress ip = WiFi.localIP();

  Serial.print("IP Address: ");

  Serial.println(ip);

  // print the received signal strength:

  long rssi = WiFi.RSSI();

  Serial.print("signal strength (RSSI):");

  Serial.print(rssi);

  Serial.println(" dBm");
}