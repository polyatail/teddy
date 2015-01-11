#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "math.h"
#include <CapacitiveSensor.h>
#include "printf.h"

// capacitive sensor
CapacitiveSensor cs = CapacitiveSensor(2,5);

// radio
RF24 radio(9, 10);
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// gyro
MPU6050 accelgyro(0x68);
int16_t vals[6];
float gyro[2];

// LEDs
#define DATA_PIN (6)
#define CLOCK_PIN (7)
#define LIGHT_COUNT (2)

uint32_t pixels[LIGHT_COUNT];
uint32_t eyecolor;

#define pinModeFast(x, y) pinMode(x, y)
#define digitalWriteFast(x, y) digitalWrite(x, y)

uint8_t recv_buffer[20] = {0, };
uint8_t left_color[3] = {255,0,0};
uint8_t right_color[3] = {0,255,0};
uint8_t c_color[3] = {0,0,0};

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
  
  // capacitive sensor
  cs.set_CS_AutocaL_Millis(120000);
  //cs.set_CS_Timeout_Millis(10);
  
  // gyro
  Wire.begin();
  accelgyro.initialize();
  printf(accelgyro.testConnection() ? "MPU6050 ok\n" : "MPU6050 failed\n");
  
  // radio
  radio.begin();
  radio.setRetries(0, 0);
  radio.setPayloadSize(20);
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
  
  // capacitive sensor
  float cap_value =  (float)cs.capacitiveSensor(30);
  
  // send
  float data_to_send[5] = {31337.0, gyro[0], gyro[1], cap_value, 31337.0};
  
  radio.stopListening();
  radio.write(&data_to_send, 20);
  radio.startListening();
  
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
  
    c_color[0] = (uint32_t)((tilt * (float)left_color[0]) + ((1.0-tilt) * (float)right_color[0]));
    c_color[1] = (uint32_t)((tilt * (float)left_color[1]) + ((1.0-tilt) * (float)right_color[1]));
    c_color[2] = (uint32_t)((tilt * (float)left_color[2]) + ((1.0-tilt) * (float)right_color[2]));
  }
  
  for (int i = 0; i < LIGHT_COUNT; i++)
  {
    set_pixel_rgb(i, c_color[2], c_color[1], c_color[0]);
  }
  
  show();

  // receive
  if (radio.available())
  {
    radio.read(&recv_buffer, 20);

    //printf("%d %d %d %d %d %d %d %d\n", recv_buffer[0], recv_buffer[1], recv_buffer[2], recv_buffer[3], recv_buffer[4], recv_buffer[5], recv_buffer[6], recv_buffer[7]);

    if (recv_buffer[0] = 137 && recv_buffer[7] == 137)
    {
      memcpy(&left_color, recv_buffer+1, 3);
      memcpy(&right_color, recv_buffer+4, 3);

      if (left_color[0] == 1 && left_color[1] == 2 && left_color[2] == 3)
      {
        idle = 1;
      } else {
        idle = 0;
      }
    }
  }
  
  delay(10);
}
