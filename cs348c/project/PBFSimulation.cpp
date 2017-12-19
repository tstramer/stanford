#include "PBFSimulation.hpp"

ofFloatColor lightAmbient = ofColor(100, 100, 100);
ofFloatColor lightDiffuse = ofColor(200, 200, 200);
ofFloatColor lightSpecular = ofColor(255, 255, 255);
float3 lightPos = float3(0, 0, 0);

float cameraDistance = 2.5;
float cameraNearClip = .1;
float lineWidth = 5;
float pointSize = 21;

PBFSimulation::PBFSimulation(Parameters& _params, msa::OpenCL& _openCL, ofEasyCam& _camera, float3 _bounds) :
    params(_params),
    openCL(_openCL),
    camera(_camera),
    bounds(_bounds),
    paused(true) {
    newBounds = bounds;
}

void PBFSimulation::init() {
    initGL();
    initCL();
    initKernels();
}

void PBFSimulation::initGL() {
    ofSetFrameRate(60);
    ofEnableDepthTest();
    glPointSize(pointSize);
    ofSetLineWidth(lineWidth);

    pointLight.setDiffuseColor(lightDiffuse);
    pointLight.setSpecularColor(lightSpecular);
    pointLight.setAmbientColor(lightAmbient);
    pointLight.setPosition(lightPos);
    
    camera.setDistance(cameraDistance);
    camera.setNearClip(cameraNearClip);
}

void PBFSimulation::initCL() {
    openCL.setupFromOpenGL();

    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    this->particlePosVBO.setVertexData((const float*)0, 4, params.numParticles, GL_STATIC_DRAW, sizeof(float) * 4);
    
    paramsBuffer.initBuffer(1, &params);
    particlesBuffer.initBuffer(params.numParticles);
    positionsBuffer.initFromGLObject(particlePosVBO.getVertId(), params.numParticles);
    
    binParticlesBuffer.initBuffer(params.numBins());
    for (int i = 0; i < params.numBins(); i++) {
        binParticlesBuffer[i].count = 0;
    }
    
    for(int i=0; i < params.numParticles; i++) {
        Particle &p = particlesBuffer[i];
        p.vel.set(0, 0, 0, 0);
        p.predPos.set(0, 0, 0, 0);
        p.pos.set(ofRandom(0, bounds.x), ofRandom(0, bounds.y), ofRandom(0, bounds.z), 0);
        for (int i = 0; i < MAX_NEIGHBORS; i++) {
            p.neighbors[i] = -1;
        }
    }

    particlesBuffer.writeToDevice();
    positionsBuffer.writeToDevice();
    paramsBuffer.writeToDevice();
    binParticlesBuffer.writeToDevice();
}

void PBFSimulation::initKernels() {
    openCL.loadProgramFromFile("Particle.cl");

    openCL.loadKernel("resetState");
    openCL.kernel("resetState")->setArg(0, particlesBuffer);

    openCL.loadKernel("predictPosition");
    openCL.kernel("predictPosition")->setArg(0, paramsBuffer);
    openCL.kernel("predictPosition")->setArg(1, particlesBuffer);
    openCL.kernel("predictPosition")->setArg(2, bounds);
    
    openCL.loadKernel("calculateParticleBin");
    openCL.kernel("calculateParticleBin")->setArg(0, paramsBuffer);
    openCL.kernel("calculateParticleBin")->setArg(1, particlesBuffer);
    openCL.kernel("calculateParticleBin")->setArg(2, bounds);

    openCL.loadKernel("assignParticlesToBins");
    openCL.kernel("assignParticlesToBins")->setArg(0, paramsBuffer);
    openCL.kernel("assignParticlesToBins")->setArg(1, particlesBuffer);
    openCL.kernel("assignParticlesToBins")->setArg(2, binParticlesBuffer);

    openCL.loadKernel("assignParticleNeighbors");
    openCL.kernel("assignParticleNeighbors")->setArg(0, paramsBuffer);
    openCL.kernel("assignParticleNeighbors")->setArg(1, particlesBuffer);
    openCL.kernel("assignParticleNeighbors")->setArg(2, binParticlesBuffer);

    openCL.loadKernel("calculateLambdas");
    openCL.kernel("calculateLambdas")->setArg(0, paramsBuffer);
    openCL.kernel("calculateLambdas")->setArg(1, particlesBuffer);

    openCL.loadKernel("calculatePositionChanges");
    openCL.kernel("calculatePositionChanges")->setArg(0, paramsBuffer);
    openCL.kernel("calculatePositionChanges")->setArg(1, particlesBuffer);
    openCL.kernel("calculatePositionChanges")->setArg(2, bounds);

    openCL.loadKernel("updatePredictedPositions");
    openCL.kernel("updatePredictedPositions")->setArg(0, paramsBuffer);
    openCL.kernel("updatePredictedPositions")->setArg(1, particlesBuffer);
    openCL.kernel("updatePredictedPositions")->setArg(2, bounds);

    openCL.loadKernel("calculateVorticity");
    openCL.kernel("calculateVorticity")->setArg(0, paramsBuffer);
    openCL.kernel("calculateVorticity")->setArg(1, particlesBuffer);

    openCL.loadKernel("updateVelocities");
    openCL.kernel("updateVelocities")->setArg(0, paramsBuffer);
    openCL.kernel("updateVelocities")->setArg(1, particlesBuffer);
    openCL.kernel("updateVelocities")->setArg(2, positionsBuffer);

    openCL.loadKernel("updatePosition");
    openCL.kernel("updatePosition")->setArg(0, paramsBuffer);
    openCL.kernel("updatePosition")->setArg(1, particlesBuffer);
    openCL.kernel("updatePosition")->setArg(2, positionsBuffer);
}

void PBFSimulation::updateBounds(float3 newBounds) {
    bounds = newBounds;
    openCL.kernel("predictPosition")->setArg(2, bounds);
    openCL.kernel("calculateParticleBin")->setArg(2, bounds);
    openCL.kernel("calculatePositionChanges")->setArg(2, bounds);
    openCL.kernel("updatePredictedPositions")->setArg(2, bounds);
}

void PBFSimulation::advanceTime(double dt) {
    glFlush();
    if (paused) return;
    
    updateBounds(newBounds);

    openCL.kernel("resetState")->run1D(params.numParticles);

    openCL.kernel("predictPosition")->run1D(params.numParticles);
    
    openCL.kernel("calculateParticleBin")->run1D(params.numParticles);
    openCL.kernel("assignParticlesToBins")->run1D(params.numBins());
    openCL.kernel("assignParticleNeighbors")->run1D(params.numParticles);
    
    for (int i = 0; i < params.numSolverIterations; i++) {
        openCL.kernel("calculateLambdas")->run1D(params.numParticles);
        openCL.kernel("calculatePositionChanges")->run1D(params.numParticles);
        openCL.kernel("updatePredictedPositions")->run1D(params.numParticles);
    }
    
    openCL.kernel("calculateVorticity")->run1D(params.numParticles);
    openCL.kernel("updateVelocities")->run1D(params.numParticles);

    openCL.kernel("updatePosition")->run1D(params.numParticles);
}

void PBFSimulation::draw() {
    openCL.finish();

    ofEnableLighting();
    pointLight.enable();
    camera.begin();

    drawScene();
    drawParticles();
    
    ofDisableLighting();
    camera.end();
}

void PBFSimulation::drawScene() {
    ofSetBackgroundColor(pointLight.getDiffuseColor());
    ofSetColor(0, 0, 255);
    ofNoFill();
    ofDrawBox(bounds / 2, bounds.x, bounds.y, bounds.z);
    ofDrawAxis(2);
}

void PBFSimulation::drawParticles() {
    this->particlePosVBO.draw(GL_POINTS, 0, params.numParticles);

    openCL.finish();
    glColor3f(1, 1, 1);
    string info = "fps: " + ofToString(ofGetFrameRate()) + "\nnumber of particles: " + ofToString(params.numParticles);
    ofDrawBitmapString(info, .5, .5);
}

void PBFSimulation::togglePaused() {
    paused = !paused;
}

void PBFSimulation::incrBounds(float amt, int axis) {
    newBounds[axis] += amt;
}
