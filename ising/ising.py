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
  width = 800
  height = 600
  temperature = 50

  # initialize pygame screen
  pygame.display.init()
  
  size = (pygame.display.Info().current_w, pygame.display.Info().current_h)
  screen = pygame.display.set_mode((width, height), pygame.FULLSCREEN)
  
  screen.fill((0, 0, 0))        
  pygame.display.update()
   
  red = (255, 0, 0)
  blue = (0, 0, 255)

  # initialize matrix with random -1 and 1s
  m = numpy.random.random_integers(0, 1, size=(width, height))
  m[m==0] -= 1

  running = True

  while running:
    try:
      # pick a random element in matrix
      rand_x = random.randint(0, width-1)
      rand_y = random.randint(0, height-1)
  
      # calculate the energy to flip from -1 to 1
      e = energy(rand_x, rand_y, m)
  
      if e <= 0 or numpy.exp(-1.0 / (temperature * e)) > numpy.random.rand():
        m[rand_x,rand_y] *= -1
  
      for x in range(0, width):
        for y in range(0, height):
          pygame.draw.rect(screen, red if m[x,y] == 1 else blue, (x, y, x+1, y+1), 0)
  
      pygame.display.update()
  
      time.sleep(0.1)
    except KeyboardInterrupt:
      running = False
      pygame.quit()

    running = False

