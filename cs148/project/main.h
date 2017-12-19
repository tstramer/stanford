// main.h
#ifndef __MAIN_H__
#define __MAIN_H__

/*
  This file is used to pull in all the requirements
  for running a program with the GLEW library for
  loadng OpenGL extensions.

  Depending on how GLEW gets set up for your system,
  you may need to edit this file.
*/

/* Include-order dependency!
*
* GLEW must be included before the standard GL.h header.
*/
#ifdef __APPLE__
#define GLEW_VERSION_2_0 1
#elif defined(__linux__)
#define GLEW_VERSION_2_0 1
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#else
#define GLEW_STATIC 1
#include "GL/glew.h"
#endif

#include <string.h>
#include <stdexcept>
#include <cmath>
//
// Forward declarations
//
class SimpleImage;

//
// headers.
//
#include "simple_image.h"
#include "findGLUT.h"

#endif // __MAIN_H__
