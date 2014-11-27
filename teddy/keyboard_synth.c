#include <sys/ioctl.h>
#include <fcntl.h>
#include <boost/thread/thread.hpp>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <termios.h>
#include <portaudio.h>

#define ARRAY_SIZE(ARRAY) (int)(sizeof(ARRAY) / sizeof(ARRAY[0]))

const double nco_bits = 2 << 15;
const double rate = 48000;
const int glissando = 480;
const int dump_every = 16;
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

char getch() {                                                                     
        char buf = 0;                                                              
        struct termios old = {0};                                                  
        if (tcgetattr(0, &old) < 0)                                                
                perror("tcsetattr()");                                             
        old.c_lflag &= ~ICANON;                                                    
        old.c_lflag &= ~ECHO;                                                      
        old.c_cc[VMIN] = 1;                                                        
        old.c_cc[VTIME] = 0;                                                       
        if (tcsetattr(0, TCSANOW, &old) < 0)                                       
                perror("tcsetattr ICANON");                                        
        if (read(0, &buf, 1) < 0)                                                  
                perror ("read()");                                                 
        old.c_lflag |= ICANON;                                                     
        old.c_lflag |= ECHO;                                                       
        if (tcsetattr(0, TCSADRAIN, &old) < 0)                                     
                perror ("tcsetattr ~ICANON");                                      
        return (buf);                                                              
}            

void wave_header(int sample_rate)
{
  char header[] = {
                   0x52, 0x49, 0x46, 0x46,  // 'RIFF'
                   0x00, 0x00, 0x00, 0x00,  // filesize set to zero
                   0x57, 0x41, 0x56, 0x45,  // 'WAVE'
                   0x66, 0x6D, 0x74, 0x20,  // 'fmt '
                   0x10, 0x00, 0x00, 0x00,  // size of this chunk (16)
                   0x01, 0x00,              // audio format (1 for PCM)
                   0x02, 0x00,              // channels (2 for stereo)
                  };

  fwrite(header, 1, ARRAY_SIZE(header), stdout);

  int mod_sample_rate = sample_rate * 2 * 2;
  fwrite(&sample_rate, 1, sizeof(sample_rate), stdout);
  fwrite(&mod_sample_rate, 1, sizeof(mod_sample_rate), stdout);

  char footer[] = {
                   0x04, 0x00, 0x10, 0x00,  // 16 bits per sample
                   0x64, 0x61, 0x74, 0x61,  // 'data'
                   0xFF, 0xFF, 0xFF, 0xFF,  // size of this data
                  };

  fwrite(footer, 1, ARRAY_SIZE(footer), stdout);
}

void synth()
{
  int i;

  wave_header(rate);

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

PaStreamParameters outputParameters;
PaStream *stream;
PaError err;

/* -- initialize PortAudio -- */
err = Pa_Initialize();
if( err != paNoError ) goto error;
/* -- setup input and output -- */
outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
outputParameters.channelCount = 1;
outputParameters.sampleFormat = paInt16;
outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
outputParameters.hostApiSpecificStreamInfo = NULL;
/* -- setup stream -- */
err = Pa_OpenStream(
          &stream,
          NULL,
          &outputParameters,
          rate,
          dump_every,
          paClipOff,      /* we won't output out of range samples so don't bother clipping them */
          NULL, /* no callback, use blocking API */
          NULL ); /* no callback, so no callback userData */
if( err != paNoError ) goto error;
/* -- start stream -- */
err = Pa_StartStream( stream );
if( err != paNoError ) goto error;

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
      Pa_WriteStream(stream, frame_buf, dump_every);
//      fwrite(frame_buf, sizeof(short), dump_every, stdout);
//      fflush(stdout);
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

  error:
  return;
}

void control()
{
  double freq = 0;
  char input;

  while ((input = getch()))
  {
/*
val: a chr: 97
val: s chr: 115
val: d chr: 100
val: f chr: 102
val: g chr: 103
val: h chr: 104
val: j chr: 106
val: k chr: 107
val: l chr: 108

val: q chr: 113
val: w chr: 119
val: e chr: 101
val: r chr: 114
val: t chr: 116
val: y chr: 121
val: u chr: 117
val: i chr: 105
val: o chr: 111
val: p chr: 112
val: [ chr: 91
*/

    switch ((int)input)
    {
      case 97: freq = 220.00; break;
      case 115: freq = 233.08; break;
      case 100: freq = 246.94; break;
      case 102: freq = 261.63; break;
      case 103: freq = 277.18; break;
      case 104: freq = 293.66; break;
      case 106: freq = 311.13; break;
      case 107: freq = 329.63; break;
      case 108: freq = 349.23; break;
      case 113: freq = 369.99; break;
      case 119: freq = 392.00; break;
      case 101: freq = 415.30; break;
      case 114: freq = 440.00; break;
      case 116: freq = 466.16; break;
      case 121: freq = 493.88; break;
      case 117: freq = 523.25; break;
      case 105: freq = 554.37; break;
      case 111: freq = 587.33; break;
      case 112: freq = 622.25; break;
      case 91: freq = 659.25; break;
    }

    l_tar_freq_updated = 1;
    l_tar_freq = freq;

    r_tar_freq_updated = 1;
    r_tar_freq = freq;
  }
}

int main()
{
  boost::thread synth_thread(synth);
  boost::thread control_thread(control);
  synth_thread.join();
  control_thread.join();
}
