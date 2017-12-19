#ifndef __MESH_H__
#define __MESH_H__

#include <boost/optional.hpp>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include <math.h>

#include "simple_image.h"

struct Point3f {
	float x, y, z;

	Point3f() { x = 0.; y = 0.; z = 0.; };
	Point3f(float _x, float _y, float _z) { x = _x; y = _y; z = _z; };

    Point3f operator+(Point3f o) { 
    	return Point3f(x+o.x, y+o.y, z+o.z); 
    }

    Point3f operator-(Point3f o) { 
    	return Point3f(x-o.x, y-o.y, z-o.z); 
    }

	Point3f operator*(Point3f o) {
		return Point3f(x * o.x, y * o.y, z * o.z);
	}

	// like '+' except mutates the current object rather than creating a new one
	void add(Point3f p) { 
		x += p.x;
		y += p.y;
		z += p.z;
	}

    Point3f cross(Point3f o) {
    	return Point3f(y * o.z - o.y * z, o.x * z - x * o.z, x * o.y - o.x * y); 
    }

    float magnitude() {
		return sqrt(x * x + y * y + z * z);
    }

    Point3f normalize() {
		float mag = magnitude();
		return Point3f(x / mag, y / mag, z / mag);
    }

	bool operator==(const Point3f& o) {
		return x == o.x && y == o.y && z == o.z;
	}

	std::string toString() { 
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
	}
};

struct BBox3f {
	Point3f min;
	Point3f max;

	BBox3f() { min = Point3f(); max = Point3f(); }

	BBox3f(Point3f _min, Point3f _max) { min = _min; max = _max; }

	bool intersects(BBox3f o) {
		if (min.x > o.max.x || max.x < o.min.x) {
			return false;
		}
		if (min.y > o.max.y || max.y < o.min.y) {
			return false;
		}
		if (min.z > o.max.z || max.z < o.min.z) {
			return false;
		}
		return true;
	}

	void partition(std::vector<BBox3f> &bboxes, int numX, int numY, int numZ) {
		Point3f size = max - min;
		float incrX = size.x / (float)numX;
		float incrY = size.y / (float)numY;
		float incrZ = size.z / (float)numZ;
		for (float currX = min.x; currX < max.x; currX += incrX) {
			for (float currY = min.y; currY < max.y; currY += incrY) {
				for (float currZ = min.z; currZ < max.z; currZ += incrZ) {
					Point3f subMin = Point3f(currX, currY, currZ);
					Point3f subMax = Point3f(currX + incrX, currY + incrY, currZ + incrZ);
					bboxes.push_back(BBox3f(subMin, subMax));
				}
			}
		}
	}
};

bool operator<(const Point3f& l, const Point3f& r);
bool operator<(const BBox3f& l, const BBox3f& r);

struct Triangle3f {
private:
	BBox3f bbox;
	bool hasBBoxCalc;

public:
	Point3f a, b, c;
	boost::optional<Point3f> t_a, t_b, t_c;

	Triangle3f() { 
		a = Point3f(); b = Point3f(); c = Point3f(); 
		t_a = boost::none; t_b = boost::none; t_c = boost::none;
		hasBBoxCalc = false;
	};
	Triangle3f(Point3f _a, Point3f _b, Point3f _c) { 
		a = _a; b = _b; c = _c; 
		t_a = boost::none; t_b = boost::none; t_c = boost::none;
		hasBBoxCalc = false;
	};
	Triangle3f(Point3f _a, Point3f _b, Point3f _c, Point3f _t_a, Point3f _t_b, Point3f _t_c) { 
		a = _a; b = _b; c = _c; 
		t_a = _t_a; t_b = _t_b; t_c = _t_c;
		hasBBoxCalc = false;
	};

    Triangle3f operator+(Point3f p) { 
    	return Triangle3f(a + p, b + p, c + p); 
    }

	Triangle3f operator*(Point3f o) {
		return Triangle3f(a * o, b * o, c * o);
	}

	// like '+' except mutates the current object rather than creating a new one
	void add(Point3f p) { 
		a.add(p);
		b.add(p);
		c.add(p);
	}

	Point3f normal() { 
		return (b - a).cross(c - a); 
	}

	BBox3f getBBox() {
		if (!hasBBoxCalc) {
			bbox.min = Point3f(
				std::min(std::min(a.x, b.x), c.x), 
				std::min(std::min(a.y, b.y), c.y), 
				std::min(std::min(a.z, b.z), c.z)
			);
			bbox.max = Point3f(
				std::max(std::max(a.x, b.x), c.x), 
				std::max(std::max(a.y, b.y), c.y), 
				std::max(std::max(a.z, b.z), c.z)
			);
			hasBBoxCalc = true;
		}
		return bbox;
	}
};

class Mesh {
private:
	std::vector<Point3f> vertices;
	std::vector<Point3f> text_coords;
	Point3f parseFaceEntry(char *entry);
	void calcBoundingBox();

public:
	std::vector<Triangle3f> faces;
	BBox3f bbox;
	Mesh() { vertices = std::vector<Point3f>(); text_coords = std::vector<Point3f>(); faces = std::vector<Triangle3f>(); };
	bool loadData(const std::string& filename);
	void draw();
};

#endif // __GAME_OBJECTS_H__