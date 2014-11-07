/*
   DirectFB Tutorials

   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <neo@directfb.org>.
              
   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/**
 * simple.c
 *
 * Simple fullscreen application that draws a horizontal line
 */

#include <stdio.h>
#include <unistd.h>

#include <directfb.h>

/*
 * This is the super interface, it's the entry point to all functionality.
 */
static IDirectFB *dfb = NULL;

/*
 * The primary surface, i.e. the "screen".
 * In cooperative level DFSCL_FULLSCREEN it's the surface of the primary layer.
 */
static IDirectFBSurface *primary = NULL;

/*
 * Store the width and height of the primary surface here to support all resolutions.
 */
static int screen_width  = 0;
static int screen_height = 0;

/*
 * An error checking macro for a call to DirectFB.
 * It is suitable for very simple apllications or tutorials.
 * In more sophisticated applications this general error checking should not be used.
 */
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
  /*
   * A surface description is needed to create a surface.
   */
  DFBSurfaceDescription dsc;

  /*
   * Initialize DirectFB passing argc and argv
   * to support the standard DirectFB command line options.
   *
   * DirectFB command line options will be stripped out automatically.
   */
  DFBCHECK (DirectFBInit (&argc, &argv));

  /*
   * Create the super interface.
   */
  DFBCHECK (DirectFBCreate (&dfb));

  /*
   * We want to go fullscreen,
   * the primary surface will be exclusive access to the surface of the primary layer.
   *
   * If you disable this call a window will be created implicitly,
   * no further changes needed, flipping the surface updates the window.
   */
  DFBCHECK (dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN));

  /*
   * 1. Specify which fields of the struct are set.
   * 2. Fill out fields,
   *    in this example we want to have a flippable primary surface.
   */
  dsc.flags = DSDESC_CAPS;
  dsc.caps  = DSCAPS_PRIMARY | DSCAPS_FLIPPING;
  
  /*
   * Create the primary surface by passing our surface description.
   */
  DFBCHECK (dfb->CreateSurface( dfb, &dsc, &primary ));

  /*
   * We have exclusive access to the primary layer's surface now,
   * get the width and height of the surface, i.e. the screen resolution.
   */
  DFBCHECK (primary->GetSize (primary, &screen_width, &screen_height));

  /*
   * Clear the screen by filling a rectangle with the size of the surface.
   * Default color is black, default drawing flags are DSDRAW_NOFX.
   */
  DFBCHECK (primary->FillRectangle (primary, 0, 0, screen_width, screen_height));

  /*
   * Draw a horizontal line in the middle of the screen.
   * Current color is black, so we have to set another one before.
   */
  DFBCHECK (primary->SetColor (primary, 0x80, 0x80, 0xff, 0xff));
  DFBCHECK (primary->DrawLine (primary,
			                      0, screen_height / 2,
			       screen_width - 1, screen_height / 2));

  /*
   * Now flip the whole surface to make things visible.
   */
  DFBCHECK (primary->Flip (primary, NULL, 0));

  /*
   * Wait a bit to see the result before exiting the example application.
   */
  sleep (5);

  /*
   * Cleanup in a stack like style.
   */
  primary->Release( primary );
  dfb->Release( dfb );
  
  return 23;
}

