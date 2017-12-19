#ifndef Parameters_hpp
#define Parameters_hpp

#include <stdio.h>

// Keep in sync with the copy of this in Particle.cl!
typedef struct Parameters {
    float timeStep;
    float particleRadius;
    float kernelRadius;
    int numParticles;
    
    int numBinsPerDim;
    int numSolverIterations;
    float restDensity;
    float cfmParam;
    
    float artificialPressureStrength;
    float artificialPressureRadius;
    float artificialPressurePower;
    float artificialViscosity;
    
    float vorticityParam;
    float dummy[3]; // need to make sure this is aligned to a 16 byte boundary

    int numBins(void) { return numBinsPerDim * numBinsPerDim * numBinsPerDim; }
} Parameters;

#endif /* Parameters_hpp */
