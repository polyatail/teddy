#include <sys/ioctl.h>
#include <boost/thread/thread.hpp>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <linux/soundcard.h>

#define ARRAY_SIZE(ARRAY) (int)(sizeof(ARRAY) / sizeof(ARRAY[0]))

const double nco_bits = 2 << 15;
const double rate = 48000;
const int glissando = 480;
const int dump_every = 1000;
const int stats_every = 100000;

int l_tar_freq_updated = 0,
    r_tar_freq_updated = 0;

double l_tar_freq = 440,
       r_tar_freq = 440;

double GetTimeStamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + (double)tv.tv_usec / (double)1000000;
}

void synth()
{
  int i, tmp;
  FILE * dsp = fopen("/dev/dsp", "wb");

  tmp = 2;
  ioctl(fileno(dsp), SNDCTL_DSP_CHANNELS, &tmp);

  tmp = (int)rate;
  ioctl(fileno(dsp), SNDCTL_DSP_SPEED, &tmp);

  tmp = AFMT_S16_LE;
  ioctl(fileno(dsp), SNDCTL_DSP_SETFMT, &tmp);

  fprintf(stderr, "generating LUTs... ");

  short sin_table[(int)nco_bits] = {0, };

  for (i = 0; i < (int)nco_bits; i++)
  {
    sin_table[i] = (int)(sin(2 * M_PI * ((double)i / (double)nco_bits)) * 32767);
  }

  double ramp[glissando] = {0, };

  for (i = 0; i < glissando; i++)
  {
    ramp[i] = exp(1 - 1.0 / ((double)i / (double)glissando));
  }

  fprintf(stderr, "done\n");

  double l_pre_freq = 440,
         l_cur_freq = 440;

  int l_cur_increment = ceil(nco_bits / (rate / l_cur_freq)),
      l_count = 0,
      l_trans = glissando;

  double r_pre_freq = 440,
         r_cur_freq = 440;

  int r_cur_increment = ceil(nco_bits / (rate / r_cur_freq)),
      r_count = 0,
      r_trans = glissando;

  int dump_count = 0, stats_count = 0;

  short frame_buf[dump_every] = {0, };

  double fps = 0,
         s_time = GetTimeStamp();

  while (1)
  {
    if (l_trans < glissando)
    {
      l_cur_freq = l_pre_freq * (1.0 - ramp[l_trans]) + l_tar_freq * ramp[l_trans];
      l_cur_increment = ceil(nco_bits / (rate / l_cur_freq));

      l_trans += 1;
    }

    if (l_tar_freq_updated == 1) {
      l_tar_freq_updated = 0;
      l_pre_freq = l_cur_freq;
      l_trans = 0;
    }

    if (r_trans < glissando)
    {
      r_cur_freq = r_pre_freq * (1.0 - ramp[r_trans]) + r_tar_freq * ramp[r_trans];
      r_cur_increment = ceil(nco_bits / (rate / r_cur_freq));

      r_trans += 1;
    }

    if (r_tar_freq_updated == 1) {
      r_tar_freq_updated = 0;
      r_pre_freq = r_cur_freq;
      r_trans = 0;
    }

    l_count = (l_count + l_cur_increment) % (int)nco_bits;
    r_count = (r_count + r_cur_increment) % (int)nco_bits;

    frame_buf[dump_count] = sin_table[l_count];
    frame_buf[dump_count+1] = sin_table[r_count];

    dump_count += 2;

    if (dump_count == dump_every)
    {
      fwrite(frame_buf, sizeof(short), dump_every, dsp);
      fflush(dsp);
      memset(frame_buf, 0, dump_every * sizeof(frame_buf[0]));

      dump_count = 0;
    }

    if (stats_count == stats_every)
    {
      fps = stats_every / (GetTimeStamp() - s_time);
      s_time = GetTimeStamp();

      stats_count = 0;
    } else {
      stats_count += 1;
    }

    if (stats_count % 100 == 0)
    {
      fprintf(stderr, "\rl_cur_freq: %7.02f r_cur_freq: %7.02f fps: %8.02f",
              l_cur_freq, r_cur_freq, fps);
    }
  }

  fclose(dsp);
}

void control()
{
  double freq[2] = {0, };

  while (fread(freq, sizeof(double), 2, stdin))
  {
    l_tar_freq_updated = 1;
    l_tar_freq = freq[0];

    r_tar_freq_updated = 1;
    r_tar_freq = freq[1];
  }
}

int main()
{
  boost::thread synth_thread(synth);
  boost::thread control_thread(control);
  synth_thread.join();
  control_thread.join();
}
