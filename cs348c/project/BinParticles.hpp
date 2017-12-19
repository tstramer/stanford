#ifndef BinParticles_hpp
#define BinParticles_hpp

#include <stdio.h>

#define MAX_PARTICLES_PER_BIN 3199

// Keep in sync with the copy of this in Particle.cl!
typedef struct BinParticles {
    int particles[MAX_PARTICLES_PER_BIN];
    int count;
} BinInfo;

#endif
