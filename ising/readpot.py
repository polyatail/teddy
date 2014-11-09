import sys
import spidev
import time

spi = spidev.SpiDev()
spi.open(0,0)

def ReadChannel(channel):
  adc = spi.xfer2([1,(8+channel)<<4,0])
  data = ((adc[1]&3) << 8) + adc[2]
  return data

while True:
  sys.stdout.write("%d\n" % ReadChannel(0)) 
  sys.stdout.flush()

  time.sleep(0.1)
