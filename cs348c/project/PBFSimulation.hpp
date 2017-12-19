#ifndef PBFSimulation_hpp
#define PBFSimulation_hpp

#include <stdio.h>
#include "ofMesh.h"
#include "MSAOpenCL.h"
#include "Particle.hpp"
#include "Parameters.hpp"
#include "ofVbo.h"
#include "BinParticles.hpp"

class PBFSimulation {
    
private:
    Parameters& params;
    msa::OpenCL& openCL;
    ofEasyCam& camera;

    float3 bounds;
    float3 newBounds;

    msa::OpenCLBufferManagedT<Particle>	particlesBuffer;
    msa::OpenCLBufferManagedT<float4> positionsBuffer;
    msa::OpenCLBufferManagedT<Parameters> paramsBuffer;
    msa::OpenCLBufferManagedT<BinParticles> binParticlesBuffer;
    
    ofVbo particlePosVBO;

    ofLight pointLight;

    bool paused;
    
    void initGL();
    void initCL();
    void initKernels();
    void drawScene();
    void drawParticles();
    void updateBounds(float3 newBounds);
    
public:
    PBFSimulation(Parameters& params, msa::OpenCL& openCL, ofEasyCam& camera, float3 bounds);
    void init();
    void advanceTime(double dt);
    void draw();
    void togglePaused();
    void incrBounds(float amt, int axis);
};

#endif /* PBFSimulation_hpp */
