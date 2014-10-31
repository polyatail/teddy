import struct
import SocketServer
import time
import spidev
import smbus
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

class MyTCPHandler(SocketServer.BaseRequestHandler):
  def handle(self):
    self.request.setblocking(0)

    colors = [255, 0, 0, 0, 0, 255]

    while True:
      x, y = get_vals(0x68, bus)

      x += 0.01
      y += 0.01

      self.request.sendall(struct.pack("d", x) + struct.pack("d", y))

      try:
        read = self.request.recv(6)
        colors += [struct.unpack("B", v)[0] for v in read]
      except:
        pass

      if len(colors) >= 6:
        l_color = colors[:3]
        r_color = colors[3:6]
        colors = colors[6:]

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
 
      time.sleep(0.01)

if __name__ == "__main__":
  host, port = "192.168.1.1", 9999

  bus = smbus.SMBus(1)
  bus.write_byte_data(0x68, 0x6b, 0)

  dev = spidev.SpiDev()
  dev.open(0,0)

  server = SocketServer.TCPServer((host, port), MyTCPHandler)
  server.socket.setblocking(0)
  server.serve_forever()

