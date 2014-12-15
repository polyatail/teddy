#include <stdio.h>
#include <unistd.h>
#include <RF24/RF24.h>
#include <cstdlib>
#include <cmath>
#include <float.h>

#define START_FREQ (220.0)
#define NUM_OCTAVES (2)

RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

//const uint8_t minor_pentatonic[] = {1,4,6,8,11};
//const uint8_t minor_pentatonic_i[] = {2,3,2,3,2};
//
//const uint8_t major_pentatonic[] = {1,3,5,8,10};
//const uint8_t major_pentatonic_i[] = {3,2,3,2,3};
//
//const uint8_t minor_natural[] = {1,3,4,6,8,9,11};
//const uint8_t minor_natural_i[] = {1,1,2,2,1,1,3};
//
//const uint8_t minor_harmonic[] = {1,3,4,6,8,9,12};
//const uint8_t minor_harmonic_i[] = {3,3,3,3,3,3,3};
//
//const uint8_t major[] = {1,3,5,6,8,10,12};
//const uint8_t major_i[] = {4,4,4,4,4,4,4};
//
//const uint8_t * scales[] = {minor_pentatonic, major_pentatonic, minor_natural,
//                            minor_harmonic, major};
//
//const uint8_t * intervals[] = {minor_pentatonic_i, major_pentatonic_i, minor_natural_i,
//                               minor_harmonic_i, major_i};
//
//const uint8_t scalesizes[] = {5, 5, 7, 7, 7};
//const uint8_t octaves[] = {2, 2, 2, 2, 2};
//
//const uint8_t colors[] = {255,0,0,0,255,0,
//                          255,0,0,0,0,255,
//                          255,0,0,0,255,255,
//                          255,0,0,64,128,64,
//                          255,0,0,255,0,255};
//
const int32_t song1[] = {6, 1, 13, 20};
const int32_t song1_i[] = {3, 3, 3, 3};

const int32_t song2[] = {8, 3, 1, 1};
const int32_t song2_i[] = {4, 4, 6, 4};

const int32_t song3[] = {20, 17, 13, 8, 10, 12, 13, 10, 13, 8};
const int32_t song3_i[] = {-19, -16, -12, -7, -4, -6, -7, -4, -7, -7};

const int32_t song4[] = {8, 3, 6, 1};
const int32_t song4_i[] = {3, 3, 4, 4};

const int32_t * scales[] = {song1, song2, song3, song4};
const int32_t * intervals[] = {song1_i, song2_i, song3_i, song4_i};
const int32_t scalesizes[] = {4, 4, 10, 4};
const int32_t octaves[] = {0, 0, 0, 0};
const int32_t colors[] = {255,0,0,0,255,0,
                          255,255,0,0,255,255,
                          0,0,255,0,255,0,
                          128,64,128,128,255,0};

double notes[72];
uint8_t scale_num = 0;

void make_notes(double start_freq)
{
  notes[0] = start_freq;

  for (uint32_t i = 1; i < sizeof(notes) / sizeof(notes[0]); i++)
  {
    notes[i] = notes[i-1] * pow((double)2.0, (double)(1.0/12.0));
  }
}

double gx2freq(double gx, const int32_t scale[], const int32_t interval[],
               const int32_t scalesize, int32_t octaves, int32_t channel)
{
  // divide tilt range into equal chunks and figure out which note in the scale
  // we should be playing
  int32_t precision = 1000000;
  double cutoff = 80.0;

  int32_t tilt = precision * copysign((abs(gx+1) < cutoff ? abs(gx+1) : cutoff) / cutoff, gx);
  int32_t tilt_interval = (double)(2 * precision) / (double)(scalesize * (octaves > 0 ? octaves : 1));

  int32_t min_tilt = 0;
  int32_t max_tilt = 0;

  int32_t scale_hit = 0;

  for (uint32_t i = 0; i < (octaves > 0 ? scalesize * octaves : scalesize); i++)
  {
    min_tilt = -precision + (tilt_interval * i);
    max_tilt = -precision + (tilt_interval * (i+1));

    if ((i+1) == scalesize * (octaves > 0 ? octaves : 1))
    {
      max_tilt = precision;
    }
 
    if (tilt >= min_tilt && tilt <= max_tilt)
    {
      scale_hit = i + 1;
      break; 
    }
  }

  int32_t note_to_play = 0;

  if (octaves > 0)
  {
    if (channel == 1)
      scale_hit += interval[(scale_hit-1)%scalesize];

    // go through all our notes and pick the ones that belong to this scale keeping
    // track of how many belong. end when the number that belong == scale_hit
    int32_t hit_count = 0;
  
    for (uint32_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++)
    {
      for (int32_t j = 0; j < scalesize; j++)
      {
        if ((int32_t)((i%12)+1) == scale[j]) 
        {
          hit_count += 1;
          break;
        }
      }
  
      if (hit_count == scale_hit)
      {
        note_to_play = i;
        break;
      }
    }
  } else {
    if (channel == 1)
      note_to_play += interval[scale_hit-1];

    note_to_play += scale[scale_hit-1];
  }

  if (channel == 0)
    fprintf(stderr, " tilt: %8d note: %2d l_freq: %7.2f i: %2d",
            tilt, scale_hit, notes[note_to_play], interval[(scale_hit-1)%scalesize]);
  else
    fprintf(stderr, " r_freq: %7.2f h: %2d", notes[note_to_play], scale_hit);

  return notes[note_to_play];
}

int main()
{
  float gyro[2];
  float cap[1];
  float received_data[3];
  double freq[3];

  double held_start = 0;

  double silence[2] = {0.0, 0.0};

  make_notes(START_FREQ);

  radio.begin();
  radio.setRetries(15, 15);
  radio.setPayloadSize(12);
  //radio.printDetails();

  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1, pipes[0]);

  radio.startListening();

  //uint8_t tmp[6] = {1, 2, 3, 0, 0 ,0};  // idle code
  uint8_t tmp[6] = {255, 0, 0, 0, 255, 0};  // red to green

  radio.stopListening();
  radio.write(&tmp, 6 * sizeof(tmp[0]));
  radio.startListening();

  while (1)
  {
    // receive
    if (radio.available())
    {
      radio.read(&received_data, 3 * sizeof(float));

      gyro[0] = received_data[0];
      gyro[1] = received_data[1];
      cap[0] = received_data[2];
    }

    fprintf(stderr, "\rgx: %6.2f gy: %6.2f cap: %5d vol: %5.02f scale: %2d",
            gyro[0], gyro[1], (uint32_t)cap[0], freq[2], scale_num);

    usleep(5000);

    freq[0] = gx2freq(gyro[0], scales[scale_num], intervals[scale_num], scalesizes[scale_num], octaves[scale_num], 0);
    freq[1] = gx2freq(gyro[0], scales[scale_num], intervals[scale_num], scalesizes[scale_num], octaves[scale_num], 1);
    freq[2] = (cap[0] > 400.0 ? 400.0 : (cap[0] < 20 ? 0 : cap[0]-20)) / 400.0;

    if (cap[0] < 100)
    {
      held_start = 0;
    } else {
      if (gyro[0] > -15 && gyro[0] < 15 && gyro[1] > -15 && gyro[1] < 15)
      {
        if (held_start == 0)
        {
          held_start = millis();
        }
  
        if (millis() - held_start >= 2000)
        {
          scale_num = (scale_num + 1) % (sizeof(scales) / sizeof(scales[0]));

          uint8_t tmp[12] = {0, };

          for (int i = 0; i < 6; i++)
          {
            tmp[i] = colors[(scale_num*6)+i];
          }

          radio.stopListening();

          for (int i = 0; i < 6; i++)
          {
            radio.write(&tmp, 12 * sizeof(tmp[0]));
          }

          radio.startListening();

          held_start = millis();
        }
      } else {
        held_start = 0;
      }

    }

    fwrite(freq, sizeof(double), 3, stdout);
    fflush(stdout);

    usleep(5000);
  }

  return 0;
}
