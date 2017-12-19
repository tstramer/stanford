#pragma once

#include "ofMain.h"
#include "PBFSimulation.hpp"

class ofApp : public ofBaseApp {
	
private:
    PBFSimulation *simulation;
    ofEasyCam camera;
    msa::OpenCL openCL;
    Parameters params;
    int resizeAxis;
    bool captureMode;
    int updateCount;
public:
	void setup();
	void update();
	void draw();
    void keyPressed(int key);
};




