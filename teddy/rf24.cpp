#include <stdio.h>
#include <unistd.h>
#include <RF24/RF24.h>
#include <cstdlib>
#include <cmath>
#include <float.h>

#define START_FREQ (110.0)
#define NUM_OCTAVES (3)

RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

const uint8_t minor_pentatonic[] = {1,4,6,8,11};
const uint8_t major_pentatonic[] = {1,3,5,8,10};
const uint8_t minor_natural[] = {1,3,4,6,8,9,11};
const uint8_t minor_harmonic[] = {1,3,4,6,8,9,12};
const uint8_t major[] = {1,3,5,6,8,10,12};

const uint8_t * scales[] = {minor_pentatonic, major_pentatonic, minor_natural,
                            minor_harmonic, major};

const uint8_t scalesizes[] = {5, 5, 7, 7, 7};

const uint8_t intervals[] = {2, 2, 2, 2, 2};

double notes[72];

void make_notes(double start_freq)
{
  notes[0] = start_freq;

  for (uint32_t i = 1; i < sizeof(notes) / sizeof(notes[0]); i++)
  {
    notes[i] = notes[i-1] * pow((double)2.0, (double)(1.0/12.0));
  }
}

double gx2freq(double gx, const uint8_t scale[], const uint8_t scalesize,
             uint8_t octaves, const uint8_t add)
{
  // divide tilt range into equal chunks and figure out which note in the scale
  // we should be playing
  uint32_t precision = 1000000;
  double cutoff = 80.0;

  int32_t tilt = precision * copysign((abs(gx+1) < cutoff ? abs(gx+1) : cutoff) / cutoff, gx);
  int32_t tilt_interval = (double)(2 * precision) / (double)(scalesize * octaves);

  int32_t min_tilt = 0;
  int32_t max_tilt = 0;

  uint8_t scale_hit = 0;

  for (uint32_t i = 0; i < scalesize * octaves; i++)
  {
    min_tilt = -precision + (tilt_interval * i);
    max_tilt = -precision + (tilt_interval * (i+1));

    if ((i+1) == scalesize * octaves)
    {
      max_tilt = precision;
    }
 
    if (tilt >= min_tilt && tilt <= max_tilt)
    {
      scale_hit = i + 1;
      break; 
    }
  }

  scale_hit += add;

  // go through all our notes and pick the ones that belong to this scale keeping
  // track of how many belong. end when the number that belong == scale_hit
  uint8_t hit_count = 0;
  uint32_t note_to_play = 0;

  for (uint32_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++)
  {
    for (uint32_t j = 0; j < scalesize; j++)
    {
      if ((uint8_t)((i%12)+1) == scale[j])
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

  if (add == 0)
    fprintf(stderr, "\rgx: %6.2f tilt: %8d note: %2d l_freq: %6.2f",
            gx, tilt, scale_hit, notes[note_to_play]);
  else
    fprintf(stderr, " r_freq: %6.2f", notes[note_to_play]);

  return notes[note_to_play];
}

int main()
{
  float gyro[2];
  double freq[2];

  make_notes(START_FREQ);

  radio.begin();
  radio.setRetries(15, 15);
  radio.setPayloadSize(16);
//  radio.printDetails();

  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1, pipes[0]);

  radio.startListening();

  while (1)
  {
    // receive
    if (radio.available())
    {
      radio.read(&gyro, 2 * sizeof(float));
    }

    freq[0] = gx2freq(gyro[0], scales[1], scalesizes[1], NUM_OCTAVES, 0);
    freq[1] = gx2freq(gyro[0], scales[1], scalesizes[1], NUM_OCTAVES, intervals[1]);

    fwrite(freq, sizeof(double), 2, stdout);
    fflush(stdout);

    usleep(5000);
  }

  return 0;
}
