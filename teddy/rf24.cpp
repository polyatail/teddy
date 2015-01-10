#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <RF24/RF24.h>
#include <cstdlib>
#include <cmath>
#include <float.h>

#define ARRAY_SIZE(ARRAY) (int)(sizeof(ARRAY) / sizeof(ARRAY[0]))

RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

// songs consist of blocks of five doubles:
//    delay in seconds, freq1, freq2, freq3, freq4
double san_francisco[] = {1.024722, 293.66, 349.23, 466.00, 0,
                          2.049444, 220.00, 261.63, 349.23, 0,
                          2.049444, 261.63, 311.00, 392.00, 0,
                          4.098888, 349.23, 440.00, 523.25, 0,
                          1.024722, 293.66, 349.23, 466.00, 0,
                          2.049444, 220.00, 261.63, 349.23, 0,
                          2.049444, 261.63, 311.00, 392.00, 0,
                          4.098888, 349.23, 440.00, 523.25, 0,
                          1.024722, 293.66, 349.23, 466.00, 0,
                          2.049444, 220.00, 261.63, 349.23, 0,
                          2.049444, 261.63, 311.00, 392.00, 0,
                          4.098888, 349.23, 440.00, 523.25, 0,
                          1.024722, 293.66, 349.23, 466.00, 0,
                          2.049444, 220.00, 261.63, 349.23, 0,
                          2.049444, 261.63, 311.00, 392.00, 0,
                          4.098888, 349.23, 440.00, 523.25, 0};

uint8_t colors[] = {255, 0, 0, 25, 0, 0,
                    0, 255, 0, 0, 25, 0,
                    0, 0, 255, 0, 0, 25,
                    255, 0, 255, 25, 0, 25,
                    255, 0, 0, 25, 0, 0,
                    0, 255, 0, 0, 25, 0,
                    0, 0, 255, 0, 0, 25,
                    255, 0, 255, 25, 0, 25,
                    255, 0, 0, 25, 0, 0,
                    0, 255, 0, 0, 25, 0,
                    0, 0, 255, 0, 0, 25,
                    255, 0, 255, 25, 0, 25,
                    255, 0, 0, 25, 0, 0,
                    0, 255, 0, 0, 25, 0,
                    0, 0, 255, 0, 0, 25,
                    255, 0, 255, 25, 0, 25};

double GetTimeStamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + (double)tv.tv_usec / (double)1000000;
}

double gx2freq(double gx, double chord[], uint32_t chordsize)
{
  // divide tilt range into equal chunks and figure out which note in the scale
  // we should be playing
  int32_t precision = 1000000;
  double cutoff = 80.0;

  int32_t tilt = precision * copysign((abs(gx+1) < cutoff ? abs(gx+1) : cutoff) / cutoff, gx);
  int32_t tilt_interval = (double)(2 * precision) / (double)chordsize;

  int32_t min_tilt = 0;
  int32_t max_tilt = 0;

  int32_t chord_hit = 0;

  for (uint32_t i = 0; i < chordsize; i++)
  {
    min_tilt = -precision + (tilt_interval * i);
    max_tilt = -precision + (tilt_interval * (i+1));

    if ((i+1) == chordsize)
    {
      max_tilt = precision;
    }
 
    if (tilt >= min_tilt && tilt <= max_tilt)
    {
      chord_hit = i;
      break; 
    }
  }

  fprintf(stderr, " tilt: %8d note: %2d freq: %7.2f",
          tilt, chord_hit, chord[chord_hit]);

  return chord[chord_hit];
}

int main()
{
  double chord_start = GetTimeStamp();

  uint32_t junk = 0;

  float gyro[2];
  float cap[1];

  double freq[3];

  double cur_chord[4];
  uint32_t cur_chord_size;
  uint32_t chord_pos = 1;

  // setup radio
  radio.begin();
  radio.setRetries(0, 0);
  radio.setPayloadSize(20);
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1, pipes[0]);
  radio.startListening();

  //radio.printDetails();

  // load first chord
  memcpy(&cur_chord, san_francisco + 1, 4 * sizeof(double));
  cur_chord_size = 3;

  while (1)
  {
    // receive
    while (radio.available())
    {
      float received_data[5] = {0, };

      radio.read(&received_data, 20);

      if (received_data[0] == 31337.0 && received_data[4] == 31337.0)
      {
        gyro[0] = received_data[1];
        gyro[1] = received_data[2];
        cap[0] = received_data[3];

        if (junk > 0)
        {
          fprintf(stderr, "\nreceived %d junk packets\n", junk);
          junk = 0;
        }
      } else {
        junk += 1;
      }
    }

    fprintf(stderr, "\rgx: %6.2f gy: %6.2f cap: %5d vol: %5.02f",
            gyro[0], gyro[1], (uint32_t)cap[0], freq[2]);

    usleep(5000);

    freq[0] = gx2freq(gyro[0], cur_chord, cur_chord_size);
    freq[1] = freq[0];
    freq[2] = (cap[0] > 400.0 ? 400.0 : (cap[0] < 20 ? 0 : cap[0]-20)) / 400.0;

    fwrite(freq, sizeof(double), 3, stdout);
    fflush(stdout);

    usleep(5000);

    double chord_now = GetTimeStamp();

    if (chord_now - chord_start > san_francisco[chord_pos*5])
    {
      double chord_extra = chord_now - chord_start - san_francisco[chord_pos*5];

      //fprintf(stderr, "\n%f > %f (extra %f) so chord_pos = %d\n", chord_now - chord_start, san_francisco[chord_pos*5], chord_extra, chord_pos+1);
      memcpy(&cur_chord, san_francisco + chord_pos * 5 + 1, 4 * sizeof(double));
      //fprintf(stderr, "\n%f\n", cur_chord[0]);

      cur_chord_size = 0;

      for (uint32_t i = 0; i < ARRAY_SIZE(cur_chord); i++)
      {
        if (cur_chord[i] != 0)
        {
          cur_chord_size += 1;
        }
      }

      chord_pos += 1;
      chord_start = chord_now - chord_extra;

      if (chord_pos*5 >= ARRAY_SIZE(san_francisco))
      {
        chord_pos = 0;
      }

      if (chord_pos == 1)
      {
        freq[0] = 31337;
        freq[1] = 31337;
        fwrite(freq, sizeof(double), 3, stdout);

        chord_start = GetTimeStamp();
      }

      // update eye color
      uint8_t color_to_send[20] = {137,0,0,0,0,0,0,137,0,0,0,0,0,0,0,0,0,0,0,0};

      memcpy(color_to_send + 1, colors + chord_pos * 6, 6);

      radio.stopListening();
      for (int i = 0; i < 40; i++)
      {
        radio.write(&color_to_send, 20);
      }
      radio.startListening();
    }
  }

  return 0;
}
