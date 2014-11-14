#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

#define ARRAY_SIZE(ARRAY) (sizeof(ARRAY) / sizeof(ARRAY[0]))

const int nco_bits = 65536;
const int rate = 44100;
const int glissando = 441;
const int dump_every = 1000;

const int minor_pentatonic[] = {1, 4, 6, 8, 11};
const int major_pentatonic[] = {1, 3, 5, 8, 10};
const int minor_natural[] = {1, 3, 4, 6, 8, 9, 11};
const int minor_harmonic[] = {1, 3, 4, 6, 8, 9, 12};
const int major[] = {1, 3, 5, 6, 8, 10, 12};

double GetTimeStamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + (double)tv.tv_usec / (double)1000000;
}

int wave_header(int sample_rate)
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

void scale_to_freq(const int *scale, double *freq, double starting_freq, int tones, int octaves)
{
  int i, j, k = 0;

  for (i = 0; i < 12 * octaves; i++)
  {
    for (j = 0; j < tones; j++)
    {
      if ((i % 12)+1 == scale[j])
      {
        freq[k] = starting_freq * pow(pow(2.0, 1.0 / 12.0), i);
        k += 1;
      }
    }
  }
}

int main()
{
  int i, j, k;

  double poop[4 * ARRAY_SIZE(minor_harmonic)];
  scale_to_freq(minor_harmonic, poop, 65.406, ARRAY_SIZE(minor_harmonic), 4);

  wave_header(rate);

  fprintf(stderr, "generating LUTs... ");

  short sin_table[nco_bits] = {0, };

  for (i = 0; i < nco_bits; i++)
  {
    sin_table[i] = (int)(sin(2 * M_PI * ((double)i / (double)nco_bits)) * 32767);
  }

  double ramp[glissando] = {0, };

  for (i = 0; i < glissando; i++)
  {
    ramp[i] = exp(1 - 1.0 / ((double)i / (double)glissando));
  }

  fprintf(stderr, "done\n");

  int l_pre_freq = 300,
      l_cur_freq = 300,
      l_cur_increment = nco_bits / (rate / l_cur_freq),
      l_tar_freq = 300,
      l_count = 0,
      l_trans = glissando;

  int r_pre_freq = 400,
      r_cur_freq = 400,
      r_cur_increment = nco_bits / (rate / r_cur_freq),
      r_tar_freq = 400,
      r_count = 0,
      r_trans = glissando;

  int dump_count = 0,
      empty_count = 0,
      idle_count = 1,
      idle_random = 1;

  int idle = 0;

  short frame_buf[dump_every] = {0, };

  double fps = 0,
         s_time = GetTimeStamp(),
         e_time = 0;

  int k_inc = 1;
  k = 0;

  while (1)
  {
    if (l_trans < glissando)
    {
      l_cur_freq = l_pre_freq * (1 - ramp[l_trans]) + l_tar_freq * ramp[l_trans];
      l_cur_increment = nco_bits / (rate / l_cur_freq);

      l_trans += 1;
    }

    if (r_trans < glissando)
    {
      r_cur_freq = r_pre_freq * (1 - ramp[r_trans]) + r_tar_freq * ramp[r_trans];
      r_cur_increment = nco_bits / (rate / r_cur_freq);

      r_trans += 1;
    }

    l_count = (l_count + l_cur_increment) % nco_bits;
    r_count = (r_count + r_cur_increment) % nco_bits;

    frame_buf[dump_count] = sin_table[l_count];
    frame_buf[dump_count+1] = sin_table[r_count];

    dump_count += 2;

    if (dump_count == dump_every)
    {
      fwrite(frame_buf, sizeof(short), dump_every, stdout);
      memset(frame_buf, 0, dump_every * sizeof(frame_buf[0]));

      dump_count = 0;

      fps = (dump_every / 2) / (GetTimeStamp() - s_time);
      s_time = GetTimeStamp();
    }

    if (dump_count % 100 == 0)
    {
      fprintf(stderr, "\rl_cur_freq: %4d r_cur_freq: %4d fps: %12.02f empty: %d",
              l_cur_freq, r_cur_freq, fps, empty_count);
    }

    if (idle_count == rate * 0.1)
    {
      l_pre_freq = l_cur_freq;
      l_tar_freq = poop[k];
      l_trans = 0;

      r_pre_freq = r_cur_freq;
      r_tar_freq = poop[k+3];
      r_trans = 0;

      k += k_inc;

      if (k+3 >= ARRAY_SIZE(poop) || k == 0)
      {
        k_inc *= -1;
        k += k_inc;
      }

      idle_count = 0;
    } else {
      idle_count += 1;
    }
  }
}
//def listener():
//  scales = {"minor_pentatonic":     (1,4,6,8,11),
//#            "major_pentatonic":     (1,3,5,8,10),
//#            "minor_natural":        (1,3,4,6,8,9,11),
//            "minor_harmonic":       (1,3,4,6,8,9,12),
//            "major":                (1,3,5,6,8,10,12)}
//
//  colors = {"minor_pentatonic":     (255,0,0,0,255,0),
//#            "major_pentatonic":     (255,0,0,0,0,255),
//#            "minor_natural":        (255,0,0,0,255,255),
//            "minor_harmonic":       (255,0,0,64,128,64),
//            "major":                (255,0,0,255,0,255),
//            "all_freqs":            (255,0,0,0,0,255)}
//
//  scale = "all_freqs"
//  add = 2
//
//  def connect():
//    global gyro
//
//    gyro = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
//
//    gyro.settimeout(1)
//
//    try:
//      result = gyro.connect(("192.168.1.1", 9999))
//    except:
//      result = False
//
//    return result
//
//  def freqmod(x, base, add = 0):
//    if x < 0:
//      freq = base - ((abs(x+1) if abs(x+1) < 70 else 70)/70) * 220
//    elif x > 0:
//      freq = base + ((abs(x+1) if abs(x+1) < 70 else 70)/70) * 440
//
//    return all_scales[scale][min(range(len(all_scales[scale])),
//                                 key=lambda i: abs(all_scales[scale][i]-freq)) + add]
//
//  all_scales = generate_scales(scales)
//  keychange_tstamp = time.time()
//  idle = False
//
//  x, y = 0.1, 0.1
//  vals = [0] * 100
//  dy_hist = [0] * 1000
//
//  no_data_counter = 0
//
//  last_left_freq = 0
//  last_right_freq = 0
//
//  connect()
//
//  while True:
//    s_read, _, _ = select.select([gyro], [], [], 0)
//
//    if len(s_read) > 0:
//      data = gyro.recv(16)
//      no_data_counter = 0
//    else:
//      no_data_counter += 1
//
//      if no_data_counter == 1000:
//        sys.stderr.write("\nidle: no data from raspi\n")
//        q.put("idle")
//
//        while True:
//          if connect() != False:
//            break
//
//          time.sleep(1)
//
//        sys.stderr.write("\nwakeup: resumed connectiong to raspi\n")
//        q.put("wakeup")
//      else:
//        time.sleep(0.001)
//
//      continue
//
//    x = struct.unpack("d", data[:8])[0]
//    y = struct.unpack("d", data[8:])[0]
//
//    vals.append(y)
//    vals = vals[1:]
//
//    dy = diff(vals)[-1]
//    dy_hist.append(abs(dy))
//    dy_hist = dy_hist[1:]
//
//    if idle:
//      try:
//        gyro.send("".join([struct.pack("B", v) for v in reds[0] + reds[0]]))
//      except:
//        pass
//
//      reds = reds[1:] + [reds[0]]
//
//      # are we no longer idle?
//      if max(dy_hist) > 2:
//        idle = False
//        sys.stderr.write("\nwakeup: gyro detected movement\n")
//        q.put("wakeup")
//
//        try:
//          gyro.send("".join([struct.pack("B", v) for v in colors[scale]]))
//        except:
//          pass
//    else:
//      new_left_freq = freqmod(x, 440, 0)
//      new_right_freq = freqmod(x, 440, add)
//
//      if new_left_freq != last_left_freq:
//        last_left_freq = new_left_freq
//        q.put((0, new_left_freq))
//
//      if new_right_freq != last_right_freq:
//        last_right_freq = new_right_freq
//        q.put((1, new_right_freq))
//
//      # should we change scales?
//      if (dy > 60 or dy < -60) and time.time() - keychange_tstamp > 0.5:
//        scale = all_scales.keys()[(all_scales.keys().index(scale)+1)%len(all_scales.keys())]
//        keychange_tstamp = time.time()
//        sys.stderr.write("\nnew scale: %s\n" % scale)
//
//        try:
//          gyro.send("".join([struct.pack("B", v) for v in colors[scale]]))
//        except:
//          pass
//
//      # should we become idle?
//      if max(dy_hist) < 2:
//        reds = [(x, 0, 0) for x in range(1, 255, 5) + range(255, 1, -5)]
//        sys.stderr.write("\nidle: no movement detected from gyro\n")
//        q.put("idle")
//        idle = True
//
//    time.sleep(0.001)
//
//if __name__ == "__main__":
//  q = Queue.Queue()
//
//  s = threading.Thread(target=synth)
//  l = threading.Thread(target=listener)
//
//  s.start()
//  l.start()
