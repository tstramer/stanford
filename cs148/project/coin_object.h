#ifndef __COIN_OBJECT_H__
#define __COIN_OBJECT_H__

#include "game_object.h"
#include "scene.h"

#define INITIAL_DIRECTION 91

#define COIN_SCALE Point3f( .1, .1, .1)
#define ROTATION_AXIS Point3f( 0.0, 1.0, 0.0)
#define COIN_ROTATION -13.25
#define COLLECT_DISTANCE 1.0
#define TOTAL_COINS 11

#define COIN_1_LOCATION Point3f( 5.0, 1.1, 10.0) //starting platform
#define COIN_2_LOCATION Point3f( -7.0, 4.3, 6.0) //third platform
#define COIN_3_LOCATION Point3f( 10.0, 2.5, -11.0) //second platform
#define COIN_4_LOCATION Point3f( -7.0, 6.0, -4.0) //circular 1
#define COIN_5_LOCATION Point3f( 0, 6.9, -10.0) //circular 2
#define COIN_6_LOCATION Point3f( -7.0, 8.0, -13.25) //circular 3
#define COIN_7_LOCATION Point3f( -10.0, 9.6, -6.0) //circular 4
#define COIN_8_LOCATION Point3f( -2.2, 11.75, -10.55) //circular 5
#define COIN_9_LOCATION Point3f( -4.0, 14.88, -11.55) //steps 1
#define COIN_10_LOCATION Point3f( -4.5, 16.71, -6.65) //steps 2
#define STAR_LOCATION Point3f( -5.5, 18.0, -9.65) //star

class CoinObject: public GameObject
{
	public:
	
		CoinObject(const std::string& _name, const std::string& meshFilePath, RGBColor _color, Point3f location);
		CoinObject(const std::string& _name, const std::string& meshFilePath, const std::string& textureImageFilename, Point3f location);

		void init( Point3f location);
		void update();
	
		void collisionHandler(GameObject *gameObject, Collision collision);
		
		static void generateCoins( Scene* scene);

	private:		
		float rotation;
		
		void rotateCoin();

};

#endif //__COIN_OBJECT_H__
