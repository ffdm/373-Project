#include <WiFi.h>
#include <esp_now.h>
#include <bb_spi_lcd.h>
#include "JPEGDEC.h"

#define CS_PIN 15
#define DC_PIN 2
#define LED_PIN 16
#define RESET_PIN 16
#define MISO_PIN 12
#define MOSI_PIN 13
#define SCK_PIN 14

// Structure example to receive data
// Must match the sender structure
#define NUM_DATA 236
typedef struct struct_message {
  uint8_t frame_num;
  uint8_t segment;
  uint8_t num_segments;
  uint32_t jpeg_len;
  uint8_t data[NUM_DATA];
} struct_message;

// Decoding and Drawing Data
JPEGDEC jpeg;
SPILCD lcd;

int drawMCUs(JPEGDRAW *pDraw)
{
  int iCount;
  iCount = pDraw->iWidth * pDraw->iHeight; // number of pixels to draw in this call
//  Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  spilcdSetPosition(&lcd, pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, DRAW_TO_LCD);
  spilcdWriteDataBlock(&lcd, (uint8_t *)pDraw->pPixels, iCount*2, DRAW_TO_LCD | DRAW_WITH_DMA);
  return 1; // returning true (1) tells JPEGDEC to continue decoding. Returning false (0) would quit decoding immediately.
} /* drawMCUs() */

// Create a struct_message called myData
struct_message myData;

unsigned int start_time;
uint8_t last_frame_num;

// keep track of which segments were received in a frame
uint8_t received_segments[20];

//char jpeg_buf[10000];
char* jpeg_buf = NULL;

bool finished_draw = true;
void decode_and_draw();

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {

  if(!finished_draw) {
    Serial.println("GOT INTERRUPTED TYPE HSIT");
  }

  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("frame num: ");
  Serial.println(myData.frame_num);
  Serial.print("segment: ");
  Serial.println(myData.segment);
  Serial.print("num_segments: ");
  Serial.println(myData.num_segments);

  memcpy(jpeg_buf + myData.segment * NUM_DATA, myData.data, NUM_DATA);

  uint8_t frame_num = myData.frame_num;
  uint8_t segment = myData.segment;
  uint8_t num_segments = myData.num_segments;

  // Clear received_segments when a new frame begins
  if(frame_num != last_frame_num) {
    for(int i = 0; i < 20; i++) {
      received_segments[i] = 0;
    }
  }

  received_segments[segment] = 1;

  // if last segment
  if(segment == num_segments - 1) {
    bool received_all = true;
    for(int i = 0; i < num_segments; i++) {
      if(received_segments[i] != 1) {
        received_all = false;
        break;
      }
    }
    if(received_all) {
      Serial.print("RECEIVED ALL SEGMENTS OF FRAME: ");
      Serial.println(frame_num);
      
      finished_draw = false;
      decode_and_draw(myData.jpeg_len);
      finished_draw = true;
      Serial.println("Finished draw");
    }
  }

  // keep track of which frame was last received
  last_frame_num = frame_num;

  // Calculate how long it took to receive the frame
  unsigned long elapsed_time = micros() - start_time; // time to receive the segment
  float frame_rate = ((float) 1000000 / (float) elapsed_time) / (float) myData.num_segments;
  Serial.print("elapsed time: ");
  Serial.print(elapsed_time);
  Serial.print(" microseconds, framerate: ");
  Serial.println(frame_rate);

  start_time = micros();
}
 
void setup() {
  jpeg_buf = (char*) malloc(8000);

  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

  spilcdInit(&lcd, LCD_ILI9341, FLAGS_NONE, 10000000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);
  spilcdSetOrientation(&lcd, LCD_ORIENTATION_90);
  spilcdFill(&lcd, 0, DRAW_TO_LCD); // erase display to black
  delay(1000);
}
 
void loop() {

}

void decode_and_draw(int size) {
  void* new_array = malloc (size);
  memcpy(new_array, jpeg_buf, size);
  Serial.println("mewo");
  if(jpeg.openRAM((uint8_t*) new_array, size, drawMCUs)) {
    Serial.println("hemlo");
    jpeg.setPixelType(RGB565_BIG_ENDIAN);
    jpeg.decode(120,100,JPEG_SCALE_HALF);
    jpeg.close();
  }
  Serial.println("woof");
  free(new_array);
}