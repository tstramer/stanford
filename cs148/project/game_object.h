#ifndef __GAME_OBJECT_H__
#define __GAME_OBJECT_H__

#include <boost/optional.hpp>
#include <vector>
#include <map>

#include "findGLUT.h"
#include "mesh.h"
#include "simple_texture.h"

struct Collision {
	Triangle3f source;
	Triangle3f target;

	Collision(Triangle3f s, Triangle3f t) { source = s; target = t; } 
};

class GameObject {
protected:
	SimpleTexture *texture = NULL;
	bool _destroyed;

	boost::optional<Collision> getCollision(
		std::map<BBox3f, std::vector<Triangle3f> *> partitionToTriangles1,
		std::map<BBox3f, std::vector<Triangle3f> *> partitionToTriangles2
	);

public:
	Mesh mesh;
	Point3f t; // translation vector
	Point3f s; // scale vector
	Point3f r; float rotationAngle; // rotation axis and angle
	RGBColor color;
	std::string name;
	std::map<BBox3f, std::vector<Triangle3f> *> partitionToTriangles;

	GameObject(
		const std::string& _name, 
		const std::string& meshFilePath, 
		RGBColor _color
	);

	GameObject(
		const std::string& _name,
		const std::string& meshFilePath,
		const std::string& textureImageFilename
	);

	// common initialization code
	void init(
		const std::string& _name, 
		const std::string& meshFilePath
	); 

	void setScale(Point3f s);
	void setTranslation(Point3f _t);
	void setRotation(float _angle, Point3f _r);
	void rotate(float _angle);

	Point3f getPosition();
	BBox3f getBoundingBox();
	float distanceFrom( GameObject* otherObject);
	float horizontalDistanceFrom( GameObject* otherObject);
	float verticalDistanceFrom( GameObject* otherObject);
	bool destroyed();
	void restore();

	std::map<BBox3f, std::vector<Triangle3f> *> getPartitionToTriangles(bool useCache);

	// gets called during every OpenGL draw call.
	// should be overriden by sub-classes to define custom update behavior.
	virtual void update(void);

	// gets called during every OpenGL draw call after GameObject::update.
	// this method should not be overriden by sub-classes (use setTranslation, setRotation, etc, instead)
	void draw(void);
};

#endif // __GAME_OBJECT_H__
