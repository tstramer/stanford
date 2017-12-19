#ifndef Particle_hpp
#define Particle_hpp

#include <stdio.h>

#define MAX_NEIGHBORS 128

// Keep in sync with the copy of this in Particle.cl!
typedef struct{
    float4 vel;
    float4 pos;
    float4 predPos;
    unsigned int bin[4]; // (x-bin, y-bin, z-bin, bin-idx)
    int neighbors[MAX_NEIGHBORS];
    float lambda;
    float dummy[3]; // dummy values needed for alignment to a 16 byte boundary
    float4 dx;
    float4 vorticity;
} Particle;

#endif
