import random
import sys
import struct
import time
import spidev
import smbus
from numpy import diff
from math import sin, pi, exp, sqrt, atan2, degrees, copysign

def twos_compliment(val):
  if (val >= 0x8000):
    return -((65535 - val) + 1)
  else:
    return val

def dist(a, b):
  return sqrt((a * a) + (b * b))

def get_y_rotation(x,y,z):
  radians = atan2(x, dist(y,z))
  return -degrees(radians)

def get_x_rotation(x,y,z):
  radians = atan2(y, dist(x,z))
  return degrees(radians)

def get_vals(address, bus):
  raw_gyro_data = bus.read_i2c_block_data(address, 0x43, 6)
  raw_accel_data = bus.read_i2c_block_data(address, 0x3b, 6)

  gyro_scaled_x = twos_compliment((raw_gyro_data[0] << 8) + raw_gyro_data[1]) / 131.0
  gyro_scaled_y = twos_compliment((raw_gyro_data[2] << 8) + raw_gyro_data[3]) / 131.0
  gyro_scaled_z = twos_compliment((raw_gyro_data[4] << 8) + raw_gyro_data[5]) / 131.0

  accel_scaled_x = twos_compliment((raw_accel_data[0] << 8) + raw_accel_data[1]) / 16384.0
  accel_scaled_y = twos_compliment((raw_accel_data[2] << 8) + raw_accel_data[3]) / 16384.0
  accel_scaled_z = twos_compliment((raw_accel_data[4] << 8) + raw_accel_data[5]) / 16384.0

  rotation_x = get_x_rotation(accel_scaled_x, accel_scaled_y, accel_scaled_z)
  rotation_y = get_y_rotation(accel_scaled_x, accel_scaled_y, accel_scaled_z)

  return rotation_x, rotation_y

def send_color(red, green, blue, dev):
  bits = [0] * 50
  
  for pixel_num in range(2):
    bits.append(1)
  
    bits.extend('{0:08b}'.format(red))
    bits.extend('{0:08b}'.format(green))
    bits.extend('{0:08b}'.format(blue))
  
  # break into bytes
  bytes_out = []
  
  for i in range(0, len(bits), 8): 
    bit_slice = bits[i:i+8]
  
    if len(bit_slice) < 8:
      bit_slice += [0] * (8 - len(bit_slice))
  
    bytes_out.append(int("".join(map(str, bit_slice)), 2)) 
  
  dev.xfer2(bytes_out)

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

def freqmod(x, base, all_scales, scale, add = 0):
  if x < 0:
    freq = base - ((abs(x+1) if abs(x+1) < 70 else 70)/70) * 220
  elif x > 0:
    freq = base + ((abs(x+1) if abs(x+1) < 70 else 70)/70) * 440

  return all_scales[scale][min(range(len(all_scales[scale])),
                               key=lambda i: abs(all_scales[scale][i]-freq)) + add]

def main():
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

  l_color = [255, 0, 0]
  r_color = [0, 0, 255]

  scale = "all_freqs"
  add = 2

  all_scales = generate_scales(scales)
  keychange_tstamp = time.time()
  idle = False

  x, y = 0.1, 0.1
  vals = [0] * 100
  dy_hist = [0] * 1000

  last_left_freq = 0
  last_right_freq = 0

  while True:
    x, y = get_vals(0x68, bus)

    x += 0.01
    y += 0.01

    tilt = ((x+60) if (x+60) > 0 else 1) / 120.0

    if tilt < 0.1:
      tilt = 0
    elif tilt > 1:
      tilt = 1

    l_rgb = [(tilt * v) for v in l_color]
    r_rgb = [((1-tilt) * v) for v in r_color]

    send_color(int(l_rgb[0] + r_rgb[0]),
               int(l_rgb[1] + r_rgb[1]),
               int(l_rgb[2] + r_rgb[2]),
               dev)
 
    vals.append(y)
    vals = vals[1:]

    dy = diff(vals)[-1]
    dy_hist.append(abs(dy))
    dy_hist = dy_hist[1:]

    if idle:
      l_color = reds[0]
      r_color = reds[0]

      reds = reds[1:] + [reds[0]]

      if random.random() < 0.10:
        octave = random.randint(0, 2)
        new_freq = 2 ** octave * 110 + 2 ** octave * 110 * random.random()

        if random.random() > 0.50:
          last_left_freq = new_freq
          sys.stdout.write(struct.pack("d", new_freq))
          sys.stdout.write(struct.pack("d", last_right_freq))
        else:
          last_right_freq = new_freq
          sys.stdout.write(struct.pack("d", last_left_freq))
          sys.stdout.write(struct.pack("d", new_freq))

        sys.stdout.flush()

      # are we no longer idle?
      if max(dy_hist) > 3:
        idle = False
        sys.stderr.write("\nwakeup: gyro detected movement\n")

        l_color = colors[scale][:3]
        r_color = colors[scale][3:]

    else:
      new_left_freq = freqmod(x, 440, all_scales, scale, 0)
      new_right_freq = freqmod(x, 440, all_scales, scale, add)

      if new_left_freq != last_left_freq or \
         new_right_freq != last_right_freq:
        last_left_freq = new_left_freq
        last_right_freq = new_right_freq

        sys.stdout.write(struct.pack("d", new_left_freq))
        sys.stdout.write(struct.pack("d", new_right_freq))
        sys.stdout.flush()

      # should we change scales?
      if (dy > 60 or dy < -60) and time.time() - keychange_tstamp > 0.5:
        scale = all_scales.keys()[(all_scales.keys().index(scale)+1)%len(all_scales.keys())]
        keychange_tstamp = time.time()
        sys.stderr.write("\nnew scale: %s\n" % scale)

        l_color = colors[scale][:3]
        r_color = colors[scale][3:]

      # should we become idle?
      if max(dy_hist) < 3:
        reds = [(x, 0, 0) for x in range(1, 255, 5) + range(255, 1, -5)]
        sys.stderr.write("\nidle: no movement detected from gyro\n")
        idle = True

    time.sleep(0.001)

if __name__ == "__main__":
  bus = smbus.SMBus(1)
  bus.write_byte_data(0x68, 0x6b, 0)

  dev = spidev.SpiDev()
  dev.open(0,0)

  main()
