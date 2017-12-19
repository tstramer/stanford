#ifndef __ENEMY_OBJECT_H__
#define __ENEMY_OBJECT_H__

#include "game_object.h"
#include "scene.h"

#define INITIAL_DIRECTION 91
#define ATTACK_SPEED 0.5
#define ATTACK_DISTANCE 8.0
#define EXPLODE_DISTANCE 0.5

#define ENEMY_SCALE Point3f( .1, .1, .1)

#define ENEMY_1_LOCATION Point3f( 7.0, 2.6, -11.0)
#define ENEMY_2_LOCATION Point3f( -5.0, 4.4, 6.0)
#define ENEMY_3_LOCATION Point3f( -8.0, 9.1, -11.25)
#define ENEMY_4_LOCATION Point3f( -7.0, 12.88, -11.55)
#define BOSS_LOCATION Point3f( -6.5, 18.5, -9.5)

class EnemyObject: public GameObject
{
	public:
	
		EnemyObject(const std::string& _name, const std::string& meshFilePath, RGBColor _color, Point3f location);
		EnemyObject(const std::string& _name, const std::string& meshFilePath, const std::string& textureImageFilename, Point3f location);

		void init( Point3f location);
		void update();
	
		void collisionHandler(GameObject *gameObject, Collision collision);
		
		static void generateEnemies( Scene* scene);
		
		void setExplodeRadius( float _explodeRadius);

	private:
	
		Point3f direction;
		float speed;
		float explodeRadius;
		Point3f initialLocation;
		
		void disable();
		void explode();

};

#endif //__ENEMY_OBJECT_H__
