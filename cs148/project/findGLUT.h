// stglut.h
#ifndef __FINDGLUT_H__
#define __FINDGLUT_H__

#include "findGL.h"

//
// The path to the glut header file may differ on
// your system. Change it if needed!
//
#ifdef __APPLE__
#include <GLUT/glut.h>
#elif WIN32
#include "glut.h"
#else
#include <GL/glut.h>
#endif

#endif // __FINDGLUT_H__
