#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"
#include "esp_camera.h"

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

unsigned int frame_num = 0;
const int LED = 4;
const int LED_input = 1;

//E4:65:B8:6F:66:18
uint8_t transmiterAddress[] = {0xE4, 0x65, 0xB8, 0x6F, 0x66, 0x18};
//E4:65:B8:6F:5C:C4
uint8_t receiverAddress[] = {0xE4, 0x65, 0xB8, 0x6F, 0x5C, 0xC4};

// Structure example to send data
// Must match the receiver structure
#define NUM_DATA 236
typedef struct struct_message {
  //unsigned int buffer_length;
  uint8_t frame_num;
  uint8_t segment;
  uint8_t num_segments;
  uint32_t jpeg_len;
  uint8_t data[NUM_DATA];
} struct_message;

// Create a struct_message called myData
struct_message myData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setupLedFlash(int pin);
void setupCamera();
void outputJPG(uint8_t* buffer, size_t buffer_length);

void setup() {
  pinMode(LED_GPIO_NUM, OUTPUT);
  pinMode(LED_input, INPUT);
  Serial.begin(115200);

  // Get mac address
  WiFi.mode(WIFI_MODE_STA);
  Serial.print("Mac Address: ");
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Esp WiFi start -> not sure why this is needed
  if(esp_wifi_start() != ESP_OK) {
    Serial.println("Error in esp_wifi_start()");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // Setup camera
  setupCamera();

  // Get 1 frame and print data
  camera_fb_t* frame = esp_camera_fb_get();
  uint8_t* buffer = frame->buf;
  size_t buffer_length = frame->len;
  size_t width = frame->width;
  size_t height = frame->height;
  Serial.print("Buffer size: ");
  Serial.print(buffer_length);
  Serial.print(" bytes, img width: ");
  Serial.print(width);
  Serial.print(" img height: ");
  Serial.print(height);
  Serial.print("\n");

  // Print jpg
  outputJPG(buffer, buffer_length);
  // Return frame to camera
  esp_camera_fb_return(frame);
}

void loop() {
  // put your main code here, to run repeatedly:
  //delay(100000);
  //return;

  digitalWrite(LED, LOW);
  unsigned long start_time = micros();

  // 1. Grab a frame from the camera
  camera_fb_t* frame = esp_camera_fb_get();
  uint8_t* buffer = frame->buf;
  size_t buffer_length = frame->len;

  uint8_t num_segments = buffer_length / NUM_DATA;

  for(uint8_t segment = 0; segment < num_segments; segment++) {
    Serial.print("Sending segment ");
    Serial.println(segment);

    myData.frame_num = frame_num;
    myData.segment = segment;
    myData.num_segments = num_segments;
    myData.jpeg_len = buffer_length;
    //void *memcpy(void *, const void *, size_t)
    memcpy(&myData.data, buffer + segment * NUM_DATA, NUM_DATA);

    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }

    delay(10);
  }

  // Return frame to camera
  esp_camera_fb_return(frame);
  
  frame_num ++;

  // Calculate how long it took to send the frame
  unsigned long elapsed_time = micros() - start_time;
  float frame_rate = (float) 1000000 / (float) elapsed_time;
  Serial.print("elapsed time: ");
  Serial.print(elapsed_time);
  Serial.print(" microseconds, framerate: ");
  Serial.println(frame_rate);
}

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

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QQVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  //setupLedFlash(LED_GPIO_NUM);
#endif
}

void outputJPG(uint8_t* buffer, size_t buffer_length) {
  for(size_t i = 0; i < buffer_length; i++) {
    Serial.print("0x");
    Serial.print(buffer[i], HEX);
    Serial.print(", ");
  }
}
