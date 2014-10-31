import select
from numpy import diff
import socket
import struct
import random
import os
import time
from math import sin, pi, exp, copysign
import sys
import Queue
import threading
import subprocess

def synth():
  aplay = subprocess.Popen(["play", "--ignore-length", "-t", "wav", "-", "--buffer", "1024"], stdin=subprocess.PIPE, bufsize=0, stderr=open(os.devnull))

  nco_bits = 2 ** 16 #24
  rate = 88200
  glissando = 881
  dump_every = 1000
  idle_after = 60 * rate

  ##

  sys.stderr.write("generating LUTs... ")
  sin_table = [struct.pack("h", int(sin(2 * pi * (float(x) / nco_bits)) * 32767)) \
               for x in range(0, nco_bits)]
  ramp = [0] + [exp(1 - 1.0 / (0.6 * (x / float(glissando)))) for x in range(1, glissando)]
  sys.stderr.write("done\n")

  wave_header = [b"RIFF",
                 b"\x00\x00\x00\x00",      # file size
                 b"WAVEfmt ",
                 b"\x10\x00\x00\x00",      # size of this chunk (16)
                 b"\x01\x00",              # audio format (1 for PCM)
                 b"\x02\x00",              # number of channels (2 for stereo)
                 struct.pack("I", rate),   # sample rate
                 struct.pack("I", rate * 2 * 2),
                 struct.pack("h", 2 * 2),
                 b"\x10\x00",              # 16 bits per sample
                 b"data",
                 b"\xFF\xFF\xFF\xFF"]      # size of this data

  for h in wave_header:
    aplay.stdin.write(h)

  l_pre_freq = 300
  l_cur_freq = 300
  l_cur_increment = nco_bits / (rate / l_cur_freq)
  l_tar_freq = 300
  l_counter = 0
  l_trans = glissando

  r_pre_freq = 300
  r_cur_freq = 300
  r_cur_increment = nco_bits / (rate / r_cur_freq)
  r_tar_freq = 300
  r_counter = 0
  r_trans = glissando

  dump_count = 0
  empty_count = 0

  idle = False
  idle_count = 1
  idle_random = 1

  frame_buf = [0] * dump_every

  fps = 0
  s_time = time.time()

  while True:
    try:
      q_get = q.get(block=False)

      if q_get == "idle":
        idle = True
      elif q_get == "wakeup":
        idle = False
      elif idle:
        sys.stderr.write("\nwakeup: control thread sending data\n")
        idle = False

      empty_count = 0
    except Queue.Empty:
      q_get = False

      if idle:
        if idle_count % idle_random == 0:
          q_get = ((random.randint(0, 1), random.randint(110, 880)))
          idle_random = random.randint(1, 30) * rate / 100.0
          idle_count = 1
        else:
          idle_count += 1
      else:
        # if there's no data for idle_after cycles, go into idle mode
        if empty_count == idle_after:
          sys.stderr.write("\nidle: no data from control thread\n")
          idle = True

        empty_count += 1

    if q_get:
      if q_get[0] == 0:
        l_tar_freq = q_get[1]
        l_pre_freq = l_cur_freq
        l_trans = 0
      elif q_get[0] == 1:
        r_tar_freq = q_get[1]
        r_pre_freq = r_cur_freq
        r_trans = 0

    if l_trans < glissando:
        l_cur_freq = l_pre_freq * (1 - ramp[l_trans]) + l_tar_freq * ramp[l_trans]
        l_cur_increment = nco_bits / (rate / l_cur_freq)

        l_trans += 1

    if r_trans < glissando:
        r_cur_freq = r_pre_freq * (1 - ramp[r_trans]) + r_tar_freq * ramp[r_trans]
        r_cur_increment = nco_bits / (rate / r_cur_freq)

        r_trans += 1

    l_counter = (l_counter + l_cur_increment) % nco_bits
    r_counter = (r_counter + r_cur_increment) % nco_bits

    frame_buf[dump_count] = sin_table[int(l_counter)]
    frame_buf[dump_count+1] = sin_table[int(r_counter)]

    dump_count += 2

    if dump_count == dump_every:
      aplay.stdin.write("".join(frame_buf))
      frame_buf = [0] * dump_every
      dump_count = 0
      fps = float(dump_every) / 2 / (time.time() - s_time)
      s_time = time.time()

    if dump_count % 100 == 0:
      sys.stderr.write("\rl_cur_freq: %.01f r_cur_freq: %.01f fps: %-8.01f empty: %d" % (l_cur_freq, r_cur_freq, fps, empty_count))

def generate_scales(scales):
  # frequencies of notes across several octaves beginning with low C
  notes = [65.406]

  for i in range(65):
    notes.append(notes[i]*2**(1/12.0))

  # generate scales
  scale_freqs = {}

  for scale, kept_notes in scales.items():
    scale_freqs[scale] = []
    i = 1

    for freq in notes:
      if i in kept_notes:
        scale_freqs[scale].append(freq)

      i += 1

      if i == 13:
        i = 1

  scale_freqs["all_freqs"] = range(65, 1400)

  return scale_freqs

def listener():
  scales = {"minor_pentatonic":     (1,4,6,8,11),
#            "major_pentatonic":     (1,3,5,8,10),
#            "minor_natural":        (1,3,4,6,8,9,11),
            "minor_harmonic":       (1,3,4,6,8,9,12),
            "major":                (1,3,5,6,8,10,12)}

  colors = {"minor_pentatonic":     (255,0,0,0,255,0),
#            "major_pentatonic":     (255,0,0,0,0,255),
#            "minor_natural":        (255,0,0,0,255,255),
            "minor_harmonic":       (255,0,0,64,128,64),
            "major":                (255,0,0,255,0,255),
            "all_freqs":            (255,0,0,0,0,255)}

  scale = "all_freqs"
  add = 2

  def connect():
    global gyro

    gyro = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    gyro.settimeout(1)

    try:
      result = gyro.connect(("192.168.1.1", 9999))
    except:
      result = False

    return result

  def freqmod(x, base, add = 0):
    if x < 0:
      freq = base - ((abs(x+1) if abs(x+1) < 70 else 70)/70) * 220
    elif x > 0:
      freq = base + ((abs(x+1) if abs(x+1) < 70 else 70)/70) * 440

    return all_scales[scale][min(range(len(all_scales[scale])),
                                 key=lambda i: abs(all_scales[scale][i]-freq)) + add]

  all_scales = generate_scales(scales)
  keychange_tstamp = time.time()
  idle = False

  x, y = 0.1, 0.1
  vals = [0] * 100
  dy_hist = [0] * 1000

  no_data_counter = 0

  last_left_freq = 0
  last_right_freq = 0

  connect()

  while True:
    s_read, _, _ = select.select([gyro], [], [], 0)

    if len(s_read) > 0:
      data = gyro.recv(16)
      no_data_counter = 0
    else:
      no_data_counter += 1

      if no_data_counter == 1000:
        sys.stderr.write("\nidle: no data from raspi\n")
        q.put("idle")
  
        while True:
          if connect() != False:
            break
  
          time.sleep(1)
  
        sys.stderr.write("\nwakeup: resumed connectiong to raspi\n")
        q.put("wakeup")
      else:
        time.sleep(0.001)

      continue

    x = struct.unpack("d", data[:8])[0]
    y = struct.unpack("d", data[8:])[0]

    vals.append(y)
    vals = vals[1:]

    dy = diff(vals)[-1]
    dy_hist.append(abs(dy))
    dy_hist = dy_hist[1:]

    if idle:
      try:
        gyro.send("".join([struct.pack("B", v) for v in reds[0] + reds[0]]))
      except:
        pass

      reds = reds[1:] + [reds[0]]

      # are we no longer idle?
      if max(dy_hist) > 2:
        idle = False
        sys.stderr.write("\nwakeup: gyro detected movement\n")
        q.put("wakeup")

        try:
          gyro.send("".join([struct.pack("B", v) for v in colors[scale]]))
        except:
          pass
    else:
      new_left_freq = freqmod(x, 440, 0)
      new_right_freq = freqmod(x, 440, add)

      if new_left_freq != last_left_freq:
        last_left_freq = new_left_freq
        q.put((0, new_left_freq))

      if new_right_freq != last_right_freq:
        last_right_freq = new_right_freq
        q.put((1, new_right_freq))

      # should we change scales?
      if (dy > 60 or dy < -60) and time.time() - keychange_tstamp > 0.5:
        scale = all_scales.keys()[(all_scales.keys().index(scale)+1)%len(all_scales.keys())]
        keychange_tstamp = time.time()
        sys.stderr.write("\nnew scale: %s\n" % scale)

        try:
          gyro.send("".join([struct.pack("B", v) for v in colors[scale]]))
        except:
          pass

      # should we become idle?
      if max(dy_hist) < 2:
        reds = [(x, 0, 0) for x in range(1, 255, 5) + range(255, 1, -5)]
        sys.stderr.write("\nidle: no movement detected from gyro\n")
        q.put("idle")
        idle = True

    time.sleep(0.001)

if __name__ == "__main__":
  q = Queue.Queue()

  s = threading.Thread(target=synth)
  l = threading.Thread(target=listener)

  s.start()
  l.start()
