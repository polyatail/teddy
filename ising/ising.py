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
  temperature = 50

  screen_width = 640
  screen_height = 480
  box_width = int(numpy.floor(float(screen_width) / width))
  box_height = int(numpy.floor(float(screen_height) / height))

  # initialize pygame screen
  pygame.display.init()
  screen = pygame.display.set_mode((screen_width, screen_height), pygame.FULLSCREEN | pygame.HWSURFACE, 24)
  screen.fill((0, 0, 0))        
  pygame.display.flip()

  # initialize matrix with random -1 and 1s
  m = numpy.random.random_integers(0, 1, size=(width, height))
  m[m==0] -= 1
  
  # initialize PIL image
  img = Image.new("RGB", (width, height), "black")
  pixels = img.load()
 
  red = (255, 0, 0)
  blue = (0, 0, 255)

  for x in range(0, width):
    for y in range(0, height):
      pixels[x, y] = red if m[x, y] == 1 else blue

  # main loop
  for _ in range(1000):
    try:
      # pick a random element in matrix
      rand_x = random.randint(0, width-1)
      rand_y = random.randint(0, height-1)
  
      # calculate the energy to flip from -1 to 1
      e = energy(rand_x, rand_y, m)
  
      if e <= 0 or numpy.exp(-1.0 / (temperature * e)) > numpy.random.rand():
        m[rand_x, rand_y] *= -1
  
      pixels[rand_x, rand_y] = red if m[rand_x, rand_y] == 1 else blue
    
      pygame_img = pygame.image.frombuffer(img.tostring(), (width, height), "RGB")
      pygame.transform.scale(pygame_img, (screen_width, screen_height), screen)
      pygame.display.flip()
    except KeyboardInterrupt:
      pygame.quit()
      break
  else:
    pygame.quit()

