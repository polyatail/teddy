#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <portaudio.h>
#include <sndfile.h>
#include <cmath>

#define ARRAY_SIZE(ARRAY) (int)(sizeof(ARRAY) / sizeof(ARRAY[0]))

#define FILE_NAME "sanfran_cleanloop.wav"

const int nco_bits = 2 << 15;
const int rate = 48000;
const int glissando = 960;
const int vol_glissando = 240;

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

  double ramp[glissando],
         vol_ramp[vol_glissando];

  int vol_updated,
      vol_trans;

  double pre_volume,
         cur_volume,
         tar_volume;

  uint32_t wf_fade_start;
  uint32_t wf_fade_pos;
  uint32_t wf_pos;
  uint32_t wf_size;
  short wf_data[4000000];
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
  double fade_off;

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

    if (data->vol_trans < vol_glissando)
    {
      //data->cur_volume = data->pre_volume + ((double)data->vol_trans / (double)glissando) * (data->tar_volume - data->pre_volume);
      data->cur_volume = data->pre_volume * (1.0 - data->vol_ramp[data->vol_trans]) + data->tar_volume * data->vol_ramp[data->vol_trans];
      data->vol_trans += 1;
    }

    if (data->vol_updated == 1) {
      data->vol_updated = 0;
      data->pre_volume = data->cur_volume;
      data->vol_trans = 0;
    }

    data->l_count = (data->l_count + data->l_cur_increment) % nco_bits;
    data->r_count = (data->r_count + data->r_cur_increment) % nco_bits;

    if (data->wf_fade_start == 0)
    {
      fade_off = 1.0;
    } else {
      fade_off = (double)(data->wf_fade_pos - data->wf_fade_start) / (double)(data->wf_size - data->wf_fade_start);
      fade_off = 1.0 / (1.0 + exp(-(fade_off / 0.1 - 5.0)));
    }

    *out++ = (short)(data->sin_table[data->l_count] * data->cur_volume * 0.4f) + \
             (short)(data->wf_data[data->wf_pos*2] * 0.6f * fade_off) + \
             (short)(data->wf_data[data->wf_fade_pos*2] * 0.6f * (1.0f-fade_off));
    *out++ = (short)(data->sin_table[data->r_count] * data->cur_volume * 0.4f) + \
             (short)(data->wf_data[data->wf_pos*2+1] * 0.6f * fade_off) + \
             (short)(data->wf_data[data->wf_fade_pos*2+1] * 0.6f * (1.0f-fade_off));

    data->wf_pos += 1;

    if (data->wf_fade_start > 0)
      data->wf_fade_pos += 1;

    if (data->wf_pos > data->wf_size)
      data->wf_pos = 0;

    if (data->wf_fade_pos > data->wf_size)
    {
      data->wf_fade_pos = 0;
      data->wf_fade_start = 0;
    }
  }

  //fprintf(stderr, "\nwf_pos: %d wf_fade_pos: %d wf_fade_start: %d fade_off: %f\n", data->wf_pos, data->wf_fade_pos, data->wf_fade_start, fade_off);

  return paContinue;
}

int main()
{
  TeddyData teddy;

  double freq[3] = {0, };

  fprintf(stderr, "generating LUTs... ");

  for (int i = 0; i < nco_bits; i++)
  {
    teddy.sin_table[i] = (int)(sin(2 * M_PI * ((double)i / (double)nco_bits)) * 32767);
  }

  for (int i = 0; i < glissando; i++)
  {
    teddy.ramp[i] = 1.0 / (1.0 + exp(-(((double)i / (double)glissando) / 0.1 - 5.0)));
  }

  for (int i = 0; i < vol_glissando; i++)
  {
    teddy.vol_ramp[i] = 1.0 / (1.0 + exp(-(((double)i / (double)vol_glissando) / 0.1 - 5.0)));
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

  teddy.pre_volume = 0.0;
  teddy.cur_volume = 0.0;
  teddy.tar_volume = 0.0;
  teddy.vol_updated = 0;
  teddy.vol_trans = vol_glissando;

  teddy.wf_fade_start = 0;
  teddy.wf_fade_pos = 0;
  teddy.wf_pos = 0;
  
  SF_INFO wf_info;
  wf_info.format = 0;
  SNDFILE *wf = sf_open(FILE_NAME, SFM_READ, &wf_info);

  if (!wf)
  {
    fprintf(stderr, "error opening file: %s", FILE_NAME);
    return 1;
  }

  teddy.wf_size = (uint32_t)wf_info.frames;
  sf_readf_short(wf, teddy.wf_data, teddy.wf_size);

  PaStreamParameters outputParameters;
  PaStream *stream;
  PaError err;

  err = Pa_Initialize();
  if (err != paNoError) goto error;

  outputParameters.device = 2; //Pa_GetDefaultOutputDevice();
  outputParameters.channelCount = 2;
  outputParameters.sampleFormat = paInt16;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&stream,
                      NULL,
                      &outputParameters,
                      rate,
                      0,
                      paClipOff,
                      teddy_callback,
                      &teddy);
  if (err != paNoError) goto error;

  err = Pa_StartStream(stream);
  if (err != paNoError) goto error;

  while (fread(freq, sizeof(double), 3, stdin))
  {
    //fprintf(stderr, "\n%f %f %f\n", freq[0], freq[1], freq[2]);

    if ((uint32_t)freq[0] == 31337 && (uint32_t)freq[1] == 31337)
    {
      teddy.wf_fade_start = teddy.wf_pos;
      teddy.wf_fade_pos = teddy.wf_pos;
      teddy.wf_pos = 0;
      continue;
    }

    if (teddy.l_trans == glissando)
    {
      teddy.l_tar_freq_updated = 1;
      teddy.l_tar_freq = freq[0];
    }

    if (teddy.r_trans == glissando)
    {
      teddy.r_tar_freq_updated = 1;
      teddy.r_tar_freq = freq[1];
    }

    if (teddy.vol_trans == vol_glissando)
    {
      teddy.vol_updated = 1;
      teddy.tar_volume = freq[2];
    }
  }

  error:
  fprintf(stderr, "error: %d: %s\n", err, Pa_GetErrorText(err));
  return 1;
}
