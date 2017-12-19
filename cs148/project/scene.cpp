#include "scene.h"

#include <map>
#include <vector>

#include "findGLUT.h"

#include "game_object.h"
#include "player_object.h"

Scene::Scene() {
    screen = START_SCREEN;
    SCENE_BBOX.partition(partitions, PARTITION_SIZE.x, PARTITION_SIZE.y, PARTITION_SIZE.z);
}

void Scene::update() {
    std::vector<GameObject *> *objects = screenToObjects[screen];
    for (GameObject *object: *objects) {
    	if (!object->destroyed()) {
            object->update();
    	}
    }
}

void Scene::draw() {
    std::vector<GameObject *> *objects = screenToObjects[screen];
    for (GameObject *object: *objects) {
        if (!object->destroyed()) {
            object->draw();
        }
    }
}

void Scene::addObject(int screen, GameObject *object) {
    std::vector<GameObject *> *currObjects;
    if (screenToObjects.count(screen) == 0) {
        currObjects = new std::vector<GameObject *>();
        screenToObjects.insert(std::pair<int, std::vector<GameObject *> *>(screen, currObjects));
    } else {
        currObjects = screenToObjects[screen];
    }
    currObjects->push_back(object);
}

void Scene::restore(int screen) {
    std::vector<GameObject *> *objects = screenToObjects[screen];
    for (GameObject *object: *objects) {
        object->restore();
    }
}