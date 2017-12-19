#include "ofApp.h"
#include "MSAOpenCL.h"
#include "PBFSimulation.hpp"

// Simulation parameters
#define TIME_STEP     (.0093)
#define NUM_PARTICLES (100000)
#define PARTICLE_RADIUS (.03)
#define KERNEL_RADIUS (.1)
#define NUM_BINS_PER_DIM (8)
#define NUM_SOLVER_ITERATIONS (4)
#define REST_DENSITY (8500)
#define CFM_PARAM (500)
#define ARTIFICIAL_PRESSURE_STRENGTH (.001)
#define ARTIFICIAL_PRESSURE_RADIUS (.03)
#define ARTIFICIAL_PRESSURE_POWER (4)
#define ARTIFICIAL_VISCOCITY (.0003)
#define VORTICITY_PARAM (.0001)

#define BOUNDS_INCR (.02)

void ofApp::setup(){
	ofBackground(0, 0, 0);
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(false);

    params.timeStep = TIME_STEP;
    params.numParticles = NUM_PARTICLES;
    params.particleRadius = PARTICLE_RADIUS;
    params.kernelRadius = KERNEL_RADIUS;
    params.numBinsPerDim = NUM_BINS_PER_DIM;
    params.numSolverIterations = NUM_SOLVER_ITERATIONS;
    params.restDensity = REST_DENSITY;
    params.cfmParam = CFM_PARAM;
    params.artificialPressureStrength = ARTIFICIAL_PRESSURE_STRENGTH;
    params.artificialPressureRadius = ARTIFICIAL_PRESSURE_RADIUS;
    params.artificialPressurePower = ARTIFICIAL_PRESSURE_POWER;
    params.artificialViscosity = ARTIFICIAL_VISCOCITY;
    params.vorticityParam = VORTICITY_PARAM;

    float3 bounds = float3(1, 1, 1);
    
    simulation = new PBFSimulation(params, openCL, camera, bounds);
    simulation->init();
    
    resizeAxis = 0;
    captureMode = false;
}

void ofApp::update(){
    simulation->advanceTime(TIME_STEP);
    if (captureMode) {
        ofImage img;
        img.grabScreen(0, 0 , ofGetWidth(), ofGetHeight());
        img.save("screenshots/screenshot" + std::to_string(updateCount) + ".png");
        updateCount++;
    }
}

void ofApp::draw(){
    simulation->draw();
}

void ofApp::keyPressed(int key) {
    switch(key) {
        case 's':
            simulation->togglePaused();
            break;
        case '1':
            resizeAxis = 0;
            break;
        case '2':
            resizeAxis = 1;
            break;
        case '3':
            resizeAxis = 2;
            break;
        case OF_KEY_LEFT:
            simulation->incrBounds(-BOUNDS_INCR, resizeAxis);
            break;
        case OF_KEY_RIGHT:
            simulation->incrBounds(BOUNDS_INCR, resizeAxis);
            break;
        case 'p':
            captureMode = true;
    }
}
