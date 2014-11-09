#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include <directfb.h>

static IDirectFB *dfb = NULL;
static IDirectFBSurface *primary = NULL;
static int screen_width  = 0;
static int screen_height = 0;

static int width = 160;
static int height = 120;

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
  DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_NORMAL));

  dsc.flags = DSDESC_CAPS;
  dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
  
  DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));
  DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));

  float temperature = 1.0;

  // generate color gradient (values here are a, r, g, b)
  DFBColor RED = {255, 255, 0, 0};
  DFBColor GREEN = {255, 0, 255, 0};
  DFBColor BLUE = {255, 0, 0, 255};
  DFBColor YELLOW = {255, 255, 255, 0};

  DFBColor colors[] = {RED, YELLOW, GREEN, BLUE, RED};
  DFBColor fades[256 * (sizeof(colors) / sizeof(colors[0]) - 1)];

  int i, j = -1, fade_length = sizeof(fades) / sizeof(fades[0]);
  float fade_perc;

  for (i = 0; i < fade_length; i++)
  {
    if (i % 256 == 0)
    {
      j += 1;
    }

    fade_perc = (float)(i % 256) / 256.0;

    fades[i].a = 255;
    fades[i].r = (int)(colors[j].r * (1 - fade_perc) + colors[j+1].r * fade_perc);
    fades[i].g = (int)(colors[j].g * (1 - fade_perc) + colors[j+1].g * fade_perc);
    fades[i].b = (int)(colors[j].b * (1 - fade_perc) + colors[j+1].b * fade_perc);
  }

  // figure out box width and height
  int box_width = screen_width / width;
  int box_height = screen_height / height;

  // initialize the matrix with random -1s and 1s
  int m[width][height];

  for (i = 0; i < width; i++)
  {
    for (j = 0; j < height; j++)
    {
      m[i][j] = rand() & 1 ? 1 : -1;
    }
  }

  // main loop
  int x, y, lx, rx, ly, ry, step, potval, randbox, f = -1, f1 = 0;
  float energy, randnum;

  while (1)
  {
    for (step = 0; step < 10000; step++)
    {
      randnum = (float)rand() / (float)RAND_MAX;
      randbox = (int)round(((float)rand() / (float)RAND_MAX) * width * height);

      // pick a random element in the matrix
      x = randbox % width;
      y = randbox / width;
  
      // calculate energy to flip
      if (x==0) lx = width-1; else lx = x-1;
      if (x==width-1) rx = 0; else rx = x+1;
      if (y==0) ly = height-1; else ly = y-1;
      if (y==height-1) ry = 0; else ry = y+1;

      energy = (float)copysign(2, temperature) * m[x][y] * (m[lx][y] + m[rx][y] + m[x][ly] + m[x][ry]);
  
      if (energy <= 0 || exp(copysign(1.0, temperature) * -1.0 * energy / temperature) > randnum)
      {
        m[x][y] *= -1;
      }
    }

    // try to read a new temperature from stdin
    if (scanf("%d", &potval) == 1)
    {
      temperature = (((float)potval - 512) / 512) * -3;
    }

    // draw the matrix on the screen
    f = (f + 1) % fade_length;
    f1 = (f + 64) % fade_length;

    DFBCHECK (primary->SetColor (primary, fades[f].r, fades[f].g, fades[f].b, 0xff));
    DFBCHECK (primary->FillRectangle (primary, 0, 0, screen_width, screen_height));
    DFBCHECK (primary->SetColor (primary, fades[f1].r, fades[f1].g, fades[f1].b, 0xff));
  
    for (i = 0; i < width; i++)
    {
      for (j = 0; j < height; j++)
      {
        if (m[i][j] == -1)
        {
          DFBCHECK (primary->FillRectangle (primary,
                    i*box_width, j*box_height,
                    box_width, box_height));
        }
      }
    }

    DFBCHECK (primary->Flip (primary, NULL, 0));
  }

  primary->Release(primary);
  dfb->Release(dfb);
  
  return 0;
}

