#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <portaudio.h>

#define ARRAY_SIZE(ARRAY) (int)(sizeof(ARRAY) / sizeof(ARRAY[0]))

const int nco_bits = 2 << 15;
const int rate = 48000;
const int glissando = 480;

typedef struct
{
  int l_tar_freq_updated,
      r_tar_freq_updated;

  int l_cur_increment,
      l_count,
      l_trans,
      r_cur_increment,
      r_count,
      r_trans;

  double l_pre_freq,
         l_cur_freq,
         l_tar_freq,
         r_pre_freq,
         r_cur_freq,
         r_tar_freq;

  short sin_table[nco_bits];

  double ramp[glissando];
}
TeddyData;

double GetTimeStamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + (double)tv.tv_usec / (double)1000000;
}

int teddy_callback(const void *inputBuffer, void *outputBuffer,
                   unsigned long framesPerBuffer,
                   const PaStreamCallbackTimeInfo* timeInfo,
                   PaStreamCallbackFlags statusFlags,
                   void *userData)
{
  unsigned long i;

  TeddyData *data = (TeddyData*)userData;
  short *out = (short*)outputBuffer;

  for (i = 0; i < framesPerBuffer; i++)
  {
    if (data->l_trans < glissando)
    {
      data->l_cur_freq = data->l_pre_freq * (1.0 - data->ramp[data->l_trans]) + data->l_tar_freq * data->ramp[data->l_trans];
      data->l_cur_increment = ceil((double)nco_bits / ((double)rate / data->l_cur_freq));

      data->l_trans += 1;
    }

    if (data->l_tar_freq_updated == 1) {
      data->l_tar_freq_updated = 0;
      data->l_pre_freq = data->l_cur_freq;
      data->l_trans = 0;
    }

    if (data->r_trans < glissando)
    {
      data->r_cur_freq = data->r_pre_freq * (1.0 - data->ramp[data->r_trans]) + data->r_tar_freq * data->ramp[data->r_trans];
      data->r_cur_increment = ceil((double)nco_bits / ((double)rate / data->r_cur_freq));

      data->r_trans += 1;
    }

    if (data->r_tar_freq_updated == 1) {
      data->r_tar_freq_updated = 0;
      data->r_pre_freq = data->r_cur_freq;
      data->r_trans = 0;
    }

    data->l_count = (data->l_count + data->l_cur_increment) % nco_bits;
    data->r_count = (data->r_count + data->r_cur_increment) % nco_bits;

    *out++ = data->sin_table[data->l_count];
    *out++ = data->sin_table[data->r_count];
  }

  return paContinue;
}

int main()
{
  int i;

  TeddyData teddy;

  double freq[2] = {0, };

  fprintf(stderr, "generating LUTs... ");

  for (i = 0; i < nco_bits; i++)
  {
    teddy.sin_table[i] = (int)(sin(2 * M_PI * ((double)i / (double)nco_bits)) * 32767);
  }

  for (i = 0; i < glissando; i++)
  {
    teddy.ramp[i] = exp(1 - 1.0 / ((double)i / (double)glissando));
  }

  fprintf(stderr, "done\n");

  teddy.l_tar_freq_updated = 0;
  teddy.l_pre_freq = 440;
  teddy.l_cur_freq = 440;
  teddy.l_tar_freq = 440;
  teddy.l_cur_increment = ceil((double)nco_bits / ((double)rate / teddy.l_cur_freq));
  teddy.l_count = 0;
  teddy.l_trans = glissando;

  teddy.r_tar_freq_updated = 0;
  teddy.r_pre_freq = 440;
  teddy.r_cur_freq = 440;
  teddy.r_tar_freq = 440;
  teddy.r_cur_increment = ceil((double)nco_bits / ((double)rate / teddy.r_cur_freq));
  teddy.r_count = 0;
  teddy.r_trans = glissando;

  PaStreamParameters outputParameters;
  PaStream *stream;
  PaError err;

  err = Pa_Initialize();
  if (err != paNoError) goto error;

  outputParameters.device = 2; //Pa_GetDefaultOutputDevice();
  outputParameters.channelCount = 2;
  outputParameters.sampleFormat = paInt16;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&stream,
                      NULL,
                      &outputParameters,
                      rate,
                      64,
                      paClipOff,
                      teddy_callback,
                      &teddy);
  if (err != paNoError) goto error;

  err = Pa_StartStream(stream);
  if (err != paNoError) goto error;

  while (fread(freq, sizeof(double), 2, stdin))
  {
    teddy.l_tar_freq_updated = 1;
    teddy.l_tar_freq = freq[0];

    teddy.r_tar_freq_updated = 1;
    teddy.r_tar_freq = freq[1];
  }

  error:
  fprintf(stderr, "error: %d: %s\n", err, Pa_GetErrorText(err));
  return 1;
}
