import numpy
import time
import pygame
import random

width = 100
height = 100
temperature = 0.03

screen_width = 640
screen_height = 480
bitdepth = 24

energy = lambda x, y, m: -2 * m[x][y] * ((m[x-1][y] if x > 0 else 0) + \
         (m[x+1][y] if x < width-1 else 0) + (m[x][y-1] if y > 0 else 0) + \
         (m[x][y+1] if y < height-1 else 0))

if __name__ == "__main__":
  # initialize pygame screen
  pygame.display.init()
  screen = pygame.display.set_mode((screen_width, screen_height), 0, 24)
#           pygame.FULLSCREEN | pygame.HWSURFACE, bitdepth)
  screen.fill((0, 0, 0))        
  pygame.display.flip()

  # initialize matrix with random -1 and 1s
  m = {}

  for i in range(0, width):
    m[i] = {}

    for j in range(0, height):
      m[i][j] = -1 if random.randint(0, 1) == 0 else 1
  
  # initialize small image
  img = pygame.Surface((width, height), 0, bitdepth)
 
  c_off = (9, 142, 186)
  c_on = (9, 244, 186)

  for x in range(0, width):
    for y in range(0, height):
      img.set_at((x, y), c_on if m[x][y] == 1 else c_off)

  # main loop
  for frame in range(1000):
    s_time = time.time()

    for step in range(10000):
      # pick a random element in matrix
      rand_x = numpy.random.random_integers(0, width-1)
      rand_y = numpy.random.random_integers(0, height-1)
 
      # calculate the energy to flip from -1 to 1
      e = energy(rand_x, rand_y, m)
  
      if e <= 0 or numpy.exp(-1.0 * e / temperature) > numpy.random.rand():
        m[rand_x][rand_y] *= -1
  
      img.set_at((rand_x, rand_y), c_on if m[rand_x][rand_y] == 1 else c_off)

    print "took %s" % (time.time() - s_time,)

    pygame.transform.scale(img, (screen_width, screen_height), screen)
    pygame.display.flip()

  pygame.quit()

