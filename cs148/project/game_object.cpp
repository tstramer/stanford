#include "game_object.h"

#include "findGLUT.h"
#include "simple_image.h"
#include "simple_texture.h"
#include "mesh.h"
#include "scene.h"

#include "triangle_triangle_intersection.h"

extern Scene *scene;

GameObject::GameObject(const std::string& _name, const std::string& meshFilePath, RGBColor _color) {
	init(_name, meshFilePath);
	color = _color;
}

GameObject::GameObject(const std::string& _name, const std::string& meshFilePath, const std::string& textureImageFilename) {
	init(_name, meshFilePath);
	SimpleImage img = SimpleImage(textureImageFilename);
	texture = new SimpleTexture(&img, SimpleTexture::kNone);
}

void GameObject::init(const std::string& _name, const std::string& meshFilePath) {
	s = Point3f(1.0, 1.0, 1.0);
	rotationAngle = 0;
	name = _name;
	mesh.loadData(meshFilePath);
	_destroyed = false;
}

void GameObject::update() {
	return;
}

void GameObject::draw() {
	glPushMatrix();

	glTranslatef(t.x, t.y, t.z);
	glScalef(s.x, s.y, s.z);

	if (rotationAngle != 0) {
		glRotatef(rotationAngle, r.x, r.y, r.z);
	}

	if (texture != NULL) { texture->bind(); }

	glColor3f(color.r, color.g, color.b);
	
	mesh.draw();

	if (texture != NULL) { texture->unBind(); }

	glPopMatrix();
}

float GameObject::distanceFrom( GameObject* otherObject)
{
	float result = 0;
	
	Point3f otherLoc = otherObject->getPosition();
	float dx = t.x - otherLoc.x;
	float dy = t.y - otherLoc.y;
	float dz = t.z - otherLoc.z;
	
	result = sqrt( dx * dx + dz * dz + dy * dy);
	
	return result;
}

float GameObject::horizontalDistanceFrom( GameObject* otherObject)
{
	float result = 0;
	
	Point3f otherLoc = otherObject->getPosition();
	float dx = t.x - otherLoc.x;
	float dz = t.z - otherLoc.z;
	
	result = sqrt( dx * dx + dz * dz);
	
	return result;
}

float GameObject::verticalDistanceFrom( GameObject* otherObject)
{
	float result = 0;
	
	Point3f otherLoc = otherObject->getPosition();
	
	result = fabs( t.y - otherLoc.y);
	
	return result;
}

bool GameObject::destroyed()
{
	return _destroyed;
}

void GameObject::restore()
{
	_destroyed = false;
}

void GameObject::setScale(Point3f _s) { s = _s; }
void GameObject::setTranslation( Point3f _t) { t = _t; }
void GameObject::setRotation(float _angle, Point3f _r) { rotationAngle = _angle; r = _r; }
void GameObject::rotate(float _angle) { rotationAngle += _angle; }

BBox3f GameObject::getBoundingBox() {
	BBox3f bbox = mesh.bbox;
	bbox.min = bbox.min + t;
	bbox.max = bbox.max + t;
	return bbox;
}

Point3f GameObject::getPosition() {
	return t;
}

std::map<BBox3f, std::vector<Triangle3f> *> GameObject::getPartitionToTriangles(bool useCache) {
	if (!useCache || partitionToTriangles.empty()) {
		partitionToTriangles.clear();
		for (auto &face : mesh.faces) {
			Triangle3f faceT = face * s;
			faceT.add(t);
			for (auto partition: scene->partitions) {
				std::vector<Triangle3f> *currTriangles;
				if (partitionToTriangles.count(partition) == 0) {
					currTriangles = new std::vector<Triangle3f>();
					partitionToTriangles.insert(std::pair<BBox3f, std::vector<Triangle3f> *>(partition, currTriangles));
				} else {
					currTriangles = partitionToTriangles[partition];
				}
				if (partition.intersects(faceT.getBBox())) {
					currTriangles->push_back(faceT);
				}
			}
		}
	}
	return partitionToTriangles;
}

boost::optional<Collision> GameObject::getCollision(
	std::map<BBox3f, std::vector<Triangle3f> *> partitionToTriangles1,
	std::map<BBox3f, std::vector<Triangle3f> *> partitionToTriangles2
) {
	for (auto partition: scene->partitions) {
		std::vector<Triangle3f> *triangles1 = partitionToTriangles1[partition];
		std::vector<Triangle3f> *triangles2 = partitionToTriangles2[partition];
		if (triangles1 != NULL && triangles2 != NULL && !triangles1->empty() && !triangles2->empty()) {
			for (auto &triangle1: *triangles1) {
				for (auto &triangle2: *triangles2) {
					if (triangle1.getBBox().intersects(triangle2.getBBox())) {
						float p1[3] = { triangle1.a.x, triangle1.a.y, triangle1.a.z };
						float p2[3] = { triangle1.b.x, triangle1.b.y, triangle1.b.z };
						float p3[3] = { triangle1.c.x, triangle1.c.y, triangle1.c.z };
						float q1[3] = { triangle2.a.x, triangle2.a.y, triangle2.a.z };
						float q2[3] = { triangle2.b.x, triangle2.b.y, triangle2.b.z };
						float q3[3] = { triangle2.c.x, triangle2.c.y, triangle2.c.z };
						if (tri_tri_overlap_test_3d( p1, p2, p3, q1, q2, q3) == 1) {
							return Collision(triangle1, triangle2);
						}
					}
				}
			}
		}
	}
	return boost::none;
}
