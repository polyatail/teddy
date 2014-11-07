import time
import numpy
import random

def energy(x, y, m):
  # given position x, y in matrix m, calculate energy to flip

  l = m[x-1,y] if x > 0 else 0
  r = m[x+1,y] if x < m.shape[0]-1 else 0

  u = m[x,y-1] if y > 0 else 0
  d = m[x,y+1] if y < m.shape[1]-1 else 0

  return -2 * m[x,y] * (l + r + u + d)

if __name__ == "__main__":
  width = 29
  height = 80
  temperature = 50

  # initialize matrix with random -1 and 1s
  m = numpy.random.random_integers(0, 1, size=(width, height))
  m[m==0] -= 1

  while True:
    # pick a random element in matrix
    rand_x = random.randint(0, width-1)
    rand_y = random.randint(0, height-1)

    # calculate the energy to flip from -1 to 1
    e = energy(rand_x, rand_y, m)

    if e <= 0 or numpy.exp(-1.0 / (temperature * e)) > numpy.random.rand():
      m[rand_x,rand_y] *= -1

    print chr(27) + "[2J"

    for x in m:
      print "".join(["." if i == -1 else "X" for i in x])

    time.sleep(0.1)
