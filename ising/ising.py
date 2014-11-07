import Image
import pygame
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
  width = 100
  height = 100
  temperature = 0.03

  screen_width = 640
  screen_height = 480
  box_width = int(numpy.floor(float(screen_width) / width))
  box_height = int(numpy.floor(float(screen_height) / height))

  # initialize pygame screen
  pygame.display.init()
  screen = pygame.display.set_mode((screen_width, screen_height), pygame.FULLSCREEN | pygame.HWSURFACE, 8)
  screen.fill((0, 0, 0))        
  pygame.display.flip()

  # initialize matrix with random -1 and 1s
  m = numpy.random.random_integers(0, 1, size=(width, height))
  m[m==0] -= 1
  
  # initialize small image
  img = pygame.Surface((width, height), 0, 8)
 
  c_off = (9, 142, 186)
  c_on= (9, 244, 186)

  for x in range(0, width):
    for y in range(0, height):
      img.set_at((x, y), c_on if m[x, y] == 1 else c_off)

  # main loop
  for frame in range(10):
    for step in range(10000):
      # pick a random element in matrix
      rand_x = random.randint(0, width-1)
      rand_y = random.randint(0, height-1)
  
      # calculate the energy to flip from -1 to 1
      e = energy(rand_x, rand_y, m)
  
      if e <= 0 or numpy.exp(-1.0 * e / temperature) > numpy.random.rand():
        m[rand_x, rand_y] *= -1
  
      img.set_at((rand_x, rand_y), c_on if m[rand_x, rand_y] == 1 else c_off)

    pygame.transform.scale(img, (screen_width, screen_height), screen)
    pygame.display.flip()

  pygame.quit()

