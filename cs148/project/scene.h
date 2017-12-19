#ifndef __SCENE_H__
#define __SCENE_H__

#include "game_object.h"
#include "mesh.h"

#include <vector>

#define SCENE_BBOX BBox3f(Point3f(-26, -2, -26), Point3f(26, 26, 26))
#define PARTITION_SIZE Point3f(2, 2, 2)

#define START_SCREEN 0
#define GAME_SCREEN 1
#define WIN_SCREEN 2
#define LOSE_SCREEN 3

class Scene {
public:
	int screen;

	std::map<int, std::vector<GameObject *> *> screenToObjects;
	std::vector<BBox3f> partitions;

	Scene();
	void update();
	void draw();

	void addObject(int screen, GameObject *gameObject);

	void restore(int screen);
};

#endif // __SCENE_H__
