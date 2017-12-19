#ifndef __FINDGL_H__
#define __FINDGL_H__

// gl.h can be in different places depending on the build
// platform.  This header attempts to encapsulate these options.
// Also gl.h on windows may have dependencies on
// definitions in windows.h 

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else

#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>
#endif

#endif // __FINDGL_H__


