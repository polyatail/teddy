#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <directfb.h>

static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static int screen_width  = 0;
static int screen_height = 0;

static int width = 80;
static int height = 60;

#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }

int main (int argc, char **argv)
{
  DFBSurfaceDescription dsc;

  DFBCHECK (DirectFBInit (&argc, &argv));
  DFBCHECK (DirectFBCreate (&dfb));
  DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));

  dsc.flags = DSDESC_CAPS;
  dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
  
  DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));
  DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));
  DFBCHECK (primary->FillRectangle (primary, 0, 0, screen_width, screen_height));

  float temperature = atof(argv[1]);

  // figure out box width and height
  int box_width = screen_width / width;
  int box_height = screen_height / height;

  // initialize the matrix with random -1s and 1s
  int m[width][height];
  int i, j;

  for (i = 0; i < width; i++)
  {
    for (j = 0; j < height; j++)
    {
      m[i][j] = rand() & 1 ? 1 : -1;
    }
  }

  // draw the matrix on the screen
  for (i = 0; i < width; i++)
  {
    for (j = 0; j < height; j++)
    {
      if (m[i][j] == -1)
      {
        DFBCHECK (primary->SetColor (primary, 0x09, 0x8e, 0xba, 0xff));
      } else {
        DFBCHECK (primary->SetColor (primary, 0x09, 0xf4, 0xba, 0xff));
      }

      DFBCHECK (primary->FillRectangle (primary,
                i*box_width, j*box_height,
                (i+1)*box_width, (i+1)+box_height));
    }
  }

  DFBCHECK (primary->Flip (primary, NULL, 0));

  // main loop
  int x, y, lx, rx, ly, ry, step;
  float energy;

  while (1)
  {
    for (step = 0; step < 10000; step++)
    {
      // pick a random element in the matrix
      x = rand() % width;
      y = rand() % height;
  
      // calculate energy to flip
      if (x==0) lx = width-1; else lx = x-1;
      if (x==width-1) rx = 0; else rx = x+1;
      if (y==0) ly = height-1; else ly = y-1;
      if (y==height-1) ry = 0; else ry = y+1;

      energy = (float)copysign(2, temperature) * m[x][y] * (m[lx][y] + m[rx][y] + m[x][ly] + m[x][ry]);
  
      if (energy <= 0 || exp(copysign(energy / temperature, temperature)) > ((float)rand() / (float)RAND_MAX))
      {
        m[x][y] *= -1;
      }
    }

    // draw the matrix on the screen
    for (i = 0; i < width; i++)
    {
      for (j = 0; j < height; j++)
      {
        if (m[i][j] == -1)
        {
          DFBCHECK (primary->SetColor (primary, 0x09, 0x8e, 0xba, 0xff));
        } else {
          DFBCHECK (primary->SetColor (primary, 0x09, 0xf4, 0xba, 0xff));
        }
  
        DFBCHECK (primary->FillRectangle (primary,
                  i*box_width, j*box_height,
                  (i+1)*box_width, (i+1)+box_height));
      }
    }

    DFBCHECK (primary->Flip (primary, NULL, 0));
  }

  primary->Release(primary);
  dfb->Release(dfb);
  
  return 23;
}

