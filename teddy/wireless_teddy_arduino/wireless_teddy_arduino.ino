#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "math.h"

// radio
RF24 radio(9, 10);
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// gyro
MPU6050 accelgyro(0x68);
int16_t vals[6];
float gyro[2];

// LEDs
#define DATA_PIN (5)
#define CLOCK_PIN (7)
#define LIGHT_COUNT (2)

uint32_t pixels[LIGHT_COUNT];
uint32_t eyecolor;

#define pinModeFast(x, y) pinMode(x, y)
#define digitalWriteFast(x, y) digitalWrite(x, y)

int32_t lr_color[6] = {255,0,0,0,255,0};
int32_t c_color[3] = {0,0,0};

bool idle = 0;
int8_t idle_inc = 2;

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

int twos_complement(int val)
{
  return (val ^ 0xFFFFFFFF) + 1;
}

double dist(float a, float b)
{
  return sqrt((a * a) + (b * b));
}

void get_vals(int16_t data[6], float *rot_x, float *rot_y)
{
  float ax, ay, az;
 
  ax = twos_complement(data[0]) / 16384.0;
  ay = twos_complement(data[1]) / 16384.0;
  az = twos_complement(data[2]) / 16384.0;
  
  *rot_y = -degrees(atan2(ax, dist(ay, az)));
  *rot_x = -degrees(atan2(ay, dist(ax, az)));
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
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1, pipes[1]);
  
  radio.startListening();
  
  radio.printDetails();

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
  get_vals(vals, &gyro[0], &gyro[1]);
  
  // send
  radio.stopListening();
  radio.write(&gyro, sizeof(gyro));
  radio.startListening();
  
  delay(1);
  
  // figure out eye color
  if (idle)
  {
    c_color[0] += idle_inc;
    c_color[1] = 0;
    c_color[2] = 0;
    
    if (c_color[0] <= 0)
    {
      idle_inc = abs(idle_inc);
      c_color[0] = 0;
    } else if (c_color[0] >= 255) {
      idle_inc = -1 * abs(idle_inc);
      c_color[0] = 255; 
    }
  } else {
    float tilt = ((gyro[0] + 60) > 0 ? (gyro[0] + 60) : 1.0) / 120.0;
    
    if (tilt < 0.1)
    {
      tilt = 0.0;
    } else if (tilt > 1.0) {
      tilt = 1.0;
    }
  
    c_color[0] = (uint8_t)((tilt * (float)lr_color[0]) + ((1.0-tilt) * (float)lr_color[3]));
    c_color[1] = (uint8_t)((tilt * (float)lr_color[1]) + ((1.0-tilt) * (float)lr_color[4]));
    c_color[2] = (uint8_t)((tilt * (float)lr_color[2]) + ((1.0-tilt) * (float)lr_color[5]));
  }
  
  for (int i = 0; i < LIGHT_COUNT; i++)
  {
    set_pixel_rgb(i, c_color[2], c_color[1], c_color[0]);
  }
  
  show();

  // receive
  if (radio.available())
  {
    radio.read(&lr_color, 6 * sizeof(lr_color[0]));
    
    if (lr_color == 0)
    {
      idle = ~idle; 
    }
  }
}
