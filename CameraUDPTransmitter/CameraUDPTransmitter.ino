#include "esp_camera.h"
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// # define DEBUG

// Which camera/receiver pair is this?
const int id = 1; // 1 or 2

// WiFi parameters
const char* ssid = "Banana Phone";
const char* password = "373 Project";
unsigned int localPorts[] = {0, 3000, 4000};
IPAddress destIPs[] = {IPAddress(0,0,0,0), IPAddress(172,20,10,6), IPAddress(172,20,10,9)};
WiFiUDP Udp;

// Camera parameters
#define FLASH_PIN 4
#define FLASH_INPUT 12
uint8_t frame_num = 0;
void setupLedFlash(int pin);
void setupCamera();
void print_JPEG(uint8_t* addr, size_t size);

void setup() {
  pinMode(FLASH_PIN, OUTPUT);
  pinMode(FLASH_INPUT, INPUT);

  Serial.begin(115200);

  setupCamera();

  // Connect to WiFi
  unsigned int localPort = localPorts[id];
  IPAddress destIP = destIPs[id];
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Attempting to connect");
  }
  Serial.println("Connected to wifi");

  Udp.begin(localPort);
}

void loop() {

  // Flashlight
  //digitalWrite(FLASH_PIN, digitalRead(FLASH_INPUT));

  // Get current time
  #ifdef DEBUG
    unsigned long start_time = micros();
  #endif

  // Grab a frame from the camera
  camera_fb_t* frame = esp_camera_fb_get();
  size_t buffer_length = frame->len;

  // Split image into multiple packets and send to other ESP32
  // Send each frame as segments of 1430 bytes + tags
  uint8_t num_segments = (buffer_length / 1430) + 1;

  for(uint8_t segment_num = 0; segment_num < num_segments; segment_num++) {
    #ifdef DEBUG
      Serial.printf("Sending frame: %d, segment: %d\n", frame_num, segment_num);
    #endif

    // Do all necessary calculations before beginning packet: keep UDP transaction as fast as possible
    uint8_t* seg_addr = frame->buf + 1430 * segment_num;

    // 1. Send some identifying information
    Udp.beginPacket(destIPs[id], localPorts[id]);
    Udp.write((uint8_t*) &buffer_length, 4); // send image length (in bytes)
    Udp.write((uint8_t*) &frame_num, 1); // send frame_num
    Udp.write((uint8_t*) &segment_num, 1); // send segment number
    Udp.write((uint8_t*) &num_segments, 1); // send number of segments to expect 

    // 2. Send next 1430 bytes of the frame
    Udp.write(seg_addr, 1430);
    Udp.endPacket();
    delay(3);
  }
  
  // Return frame to camera
  esp_camera_fb_return(frame);
  
  frame_num ++ ;

  #ifdef DEBUG
    // Calculate how long it took to send the frame
    unsigned long elapsed_time = micros() - start_time;
    float frame_rate = (float) 1000000 / (float) elapsed_time;
    Serial.printf("Elapsed time: %4f microseconds, Frame rate: %4f\n", elapsed_time, frame_rate);
  #endif
}

/*
typedef struct {
  uint8_t * buf;              //!< Pointer to the pixel data //
  size_t len;                 //!< Length of the buffer in bytes //
  size_t width;               //!< Width of the buffer in pixels //
  size_t height;              //!< Height of the buffer in pixels //
  pixformat_t format;         //!< Format of the pixel data //
  struct timeval timestamp;   //!< Timestamp since boot of the first DMA buffer of the frame //
} camera_fb_t;
*/

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QQVGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 63; // 12 was default
  config.fb_count = 2;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 63; // 12 was default
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_QQVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } 

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  //setupLedFlash(LED_GPIO_NUM);
#endif

  // Initial camera information
  camera_fb_t* frame = esp_camera_fb_get();
  uint8_t* buffer = frame->buf;
  size_t buffer_length = frame->len;
  size_t width = frame->width;
  size_t height = frame->height;

  Serial.printf("Buffer size: %d bytes, Resolution: %d x %d\n", buffer_length, width, height);
  esp_camera_fb_return(frame);
}

// Prints a JPG given an addr and file size
void print_JPEG(uint8_t* addr, size_t size) {
  Serial.println("Printing JPG contents:");
  for(size_t i = 0; i < size; i++) {

    Serial.printf("0x%x, ", addr[i]);

    if(i % 100 == 99) {
      Serial.printf("\n");
    }
  }
  Serial.printf("\n");
}
