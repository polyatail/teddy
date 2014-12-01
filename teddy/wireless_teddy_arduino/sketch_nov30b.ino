#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"

// radio
RF24 radio(9, 10);
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// gyro
MPU6050 accelgyro(0x68);
int16_t vals[6];

// LEDs
#define DATA_PIN (5)
#define CLOCK_PIN (7)
#define LIGHT_COUNT (2)

uint32_t pixels[LIGHT_COUNT];
uint32_t eyecolor;

#define pinModeFast(x, y) pinMode(x, y)
#define digitalWriteFast(x, y) digitalWrite(x, y)

static void set_pixel(uint8_t index, uint32_t color) {
  pixels[index] = color & 0x00FFFFFF;
}

static void set_pixel_rgb(uint8_t index, uint32_t r, uint32_t g, uint32_t b) {
  set_pixel(index, r | (g << 8) | (b << 16));
}

static void toggle_clock() {
  digitalWriteFast(CLOCK_PIN, HIGH);
  digitalWriteFast(CLOCK_PIN, LOW);
}

static void write_pixel(uint8_t i) {
  const uint32_t MASK = ((uint32_t)(1) << 24);
  uint32_t p = pixels[i] | MASK;
  int j = 25;
  while (j--) {
    digitalWriteFast(DATA_PIN, (p & MASK) ? HIGH : LOW);
    toggle_clock();
    p <<= 1;
  }
}

static void write_blank_pixel() {
  int j = 25;
  while (j--) {
    digitalWriteFast(DATA_PIN, 0);
    toggle_clock();
  }
}

static void show() {
  digitalWriteFast(DATA_PIN, LOW);
  for (int i = 0; i < 50; i++) {
    toggle_clock();
  }
  for (int i = 0; i < LIGHT_COUNT; ++i) {
    write_pixel(i);
  }
  write_blank_pixel();
  delay(1);
}

void setup()
{
  Serial.begin(9600);
  printf_begin();
  
  // gyro
  Wire.begin();
  accelgyro.initialize();
  printf(accelgyro.testConnection() ? "MPU6050 ok\n" : "MPU6050 failed\n");
  
  // radio
  radio.begin();
  radio.setRetries(15, 15);
  radio.setPayloadSize(16);
  radio.printDetails();
  
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1, pipes[1]);
  
  radio.startListening();

  // LEDs
  pinModeFast(DATA_PIN, OUTPUT);
  pinModeFast(CLOCK_PIN, OUTPUT);
  digitalWriteFast(DATA_PIN, LOW);
  digitalWriteFast(CLOCK_PIN, LOW);
}

void loop()
{
  // gyro
  accelgyro.getMotion6(&vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]);
  
  // send
  radio.stopListening();
  radio.write(&vals, 6 * sizeof(int16_t));
  radio.startListening();
  
  delay(10);
      
  // receive
  if (radio.available())
  {
    radio.read(&eyecolor, sizeof(uint32_t));
    
    // LEDs
    for (int i = 0; i < LIGHT_COUNT; i++)
    {
      set_pixel(i, eyecolor);
    }
    
    show();
  }
  
  delay(10);
}
