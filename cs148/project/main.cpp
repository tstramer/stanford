
#include "main.h"
#ifdef WIN32
#define ssize_t SSIZE_T
#endif

#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cmath>

#include "mesh.h"
#include "game_object.h"
#include "player_object.h"
#include "coin_object.h"
#include "enemy_object.h"
#include "scene.h"

#include "simple_texture.h"

#define STATIC_SCREEN_CAMERA_POSITION Point3f(0, 3.0, 7.5)
#define STATIC_SCREEN_CAMERA_AIM Point3f(0, 3.0, 0)

Scene *scene;

PlayerObject *player;
GameObject *environment;
CoinObject *coins;
GameObject* sky;

GameObject* startScreen;
GameObject* winScreen;
GameObject* loseScreen;

Point3f cameraNormal = Point3f(0.0, 1.0, 0.0);
Point3f cameraPositionOffset = Point3f(0.0, 0.0, 0.0);
Point3f cameraAimOffset = Point3f(0.0, 0.0, 0.0);

Point3f getCameraPosition() {
    if (scene->screen == GAME_SCREEN) {
        Point3f playerPosition = player->getPosition();
        Point3f playerDirection = player->getDirection();

        Point3f cameraPosition = Point3f(
            playerPosition.x - (playerDirection.x * 20),
            playerPosition.y + 13,
            playerPosition.z - (playerDirection.z * 20)
        );

        return cameraPositionOffset + cameraPosition;
    } else {
        return STATIC_SCREEN_CAMERA_POSITION;
    }
}

Point3f getCameraAim() {
    if (scene->screen == GAME_SCREEN) {
        return cameraAimOffset + player->getPosition();
    } else {
        return STATIC_SCREEN_CAMERA_AIM;
    }
}

void updateCamera() {
    Point3f cameraPosition = getCameraPosition();
    Point3f cameraAim = getCameraAim();

	gluLookAt(
        cameraPosition.x, cameraPosition.y, cameraPosition.z, 
        cameraAim.x,      cameraAim.y,      cameraAim.z,
        cameraNormal.x,   cameraNormal.y,   cameraNormal.z
    );
}

void displayCallback() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    scene->update();
    updateCamera();
    scene->draw();

    glutSwapBuffers();
}

void reshapeCallback(int w, int h) {
    startScreen->setScale(Point3f(w / (float) h - .02, 1.0, 1.0));
    winScreen->setScale(Point3f(w / (float) h - .02, 1.0, 1.0));
    loseScreen->setScale(Point3f(w / (float) h - .02, 1.0, 1.0));

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(30.0f, (float)w/(float)h, 0.1f, 100000.f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void setup() {
    glGenTextures(MAX_TEXTURES, SimpleTexture::getTextures());

    scene = new Scene();
    
    player = new PlayerObject("player", "objects/character_new.obj", "textures/character_new.png");
    environment = new GameObject("environment", "environments/textured_environment.obj", "textures/Environment_texture.png");
    sky = new GameObject("sky", "environments/textured_sky.obj", "textures/Sky.png"); 
    sky->setScale(Point3f( 20.0, 20.0, 20.0));
    CoinObject::generateCoins(scene);
    EnemyObject::generateEnemies(scene);
    scene->addObject(GAME_SCREEN, environment);
    scene->addObject(GAME_SCREEN, player);
    scene->addObject(GAME_SCREEN, sky);

    startScreen = new GameObject("start_screen", "environments/textured_plane.obj", "textures/Main_menu.png");
    startScreen->setScale(Point3f(1.31, 1.0, 1.0));
    scene->addObject(START_SCREEN, startScreen);

    winScreen = new GameObject("win_screen", "environments/textured_plane.obj", "textures/Win.png");
    winScreen->setScale(Point3f(1.31, 1.0, 1.0));
    scene->addObject(WIN_SCREEN, winScreen);

    loseScreen = new GameObject("lose_screen", "environments/textured_plane.obj", "textures/Lose.png");
    loseScreen->setScale(Point3f(1.31, 1.0, 1.0));
    scene->addObject(LOSE_SCREEN, loseScreen);

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glShadeModel (GL_SMOOTH);
    glEnable( GL_DEPTH_TEST);
}


void printCameraCoords() {
    Point3f cameraPosition = getCameraPosition();
    Point3f cameraAim = getCameraAim();

    std::cout << "Camera position: (" << cameraPosition.x << ", " << cameraPosition.y << ", " << cameraPosition.z << ")" << std::endl;
    std::cout << "Camera aim: (" << cameraAim.x << ", " << cameraAim.y << ", " << cameraAim.z << ")" << std::endl;
    std::cout << "Camera normal: (" << cameraNormal.x << ", " << cameraNormal.y << ", " << cameraNormal.z << ")" << std::endl;
}

void specialKeyCallback( int key, int x, int y) {
    if (scene->screen == GAME_SCREEN) {
    	switch( key)
    	{
    		case GLUT_KEY_UP: { player->step( 1.0); break; }
    		case GLUT_KEY_DOWN: { player->step( -1.0); break; }			
    		case GLUT_KEY_LEFT: { player->turnLeft(); break; }
    		case GLUT_KEY_RIGHT: { player->turnRight(); break; }	
    		default: break;
    	}
    }
	
	glutPostRedisplay();

	return;
}

void keyCallback(unsigned char key, int x, int y) {
    if (key == 'q') {
        exit(0);
        return;
    }

    if (scene->screen == GAME_SCREEN) {
        switch(key) {
            case 'q': { exit(0); break; }
            case '1': { cameraPositionOffset.x += .5; printCameraCoords(); break; }
            case '2': { cameraPositionOffset.x -= .5; printCameraCoords(); break; }
            case '3': { cameraPositionOffset.y += .5; printCameraCoords(); break; }
            case '4': { cameraPositionOffset.y -= .5; printCameraCoords(); break; }
            case '5': { cameraPositionOffset.z += .5; printCameraCoords(); break; }
            case '6': { cameraPositionOffset.z -= .5; printCameraCoords(); break; }
            case '7': { cameraAimOffset.x += .5; printCameraCoords(); break; }
            case '8': { cameraAimOffset.x -= .5; printCameraCoords(); break; }
            case '9': { cameraAimOffset.y += .5; printCameraCoords(); break; }
            case '0': { cameraAimOffset.y -= .5; printCameraCoords(); break; }
            case '-': { cameraAimOffset.z += .5; printCameraCoords(); break; }
            case '=': { cameraAimOffset.z -= .5; printCameraCoords(); break; }
            case 'r': { 
                cameraPositionOffset = Point3f(0.0, 0.0, 0.0);
                cameraAimOffset = Point3f(0.0, 0.0, 0.0);
                break;
            }
            case 'j': case ' ': { player->jump(); break; }
            default: { break; }
        }
    } else if(scene->screen == START_SCREEN) {
        switch(key) {
            case ' ': { scene->screen = GAME_SCREEN; break; }
            default: { break; }
        }
    } else if (scene->screen == WIN_SCREEN || scene->screen == LOSE_SCREEN) {
        switch(key) {
            case 'r': { 
                scene->restore(GAME_SCREEN);
                scene->screen = GAME_SCREEN;
                break; 
             }
            default: { break; }
        }
    }

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    // Initialize GLUT.
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(20, 20);
    glutInitWindowSize(1280, 960);
    glutCreateWindow("Mario Game");

    //
    // Initialize GLEW.
    //
#if !defined(__APPLE__) && !defined(__linux__)
    glewInit();
    if(!GLEW_VERSION_2_0) {
        printf("Your graphics card or graphics driver does\n"
               "\tnot support OpenGL 2.0, trying ARB extensions\n");

        if(!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader) {
            printf("ARB extensions don't work either.\n");
            printf("\tYou can try updating your graphics drivers.\n"
                   "\tIf that does not work, you will have to find\n");
            printf("\ta machine with a newer graphics card.\n");
            exit(1);
        }
    }
#endif

    setup();

    glutDisplayFunc(displayCallback);
    glutReshapeFunc(reshapeCallback);
    glutKeyboardFunc(keyCallback);
    glutSpecialFunc(specialKeyCallback);
    glutIdleFunc(displayCallback);

    glutMainLoop();
    return 0;
}
