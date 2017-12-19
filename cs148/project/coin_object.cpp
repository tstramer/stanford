#include "coin_object.h"
#include "player_object.h"

extern PlayerObject* player;

CoinObject::CoinObject(const std::string& _name, const std::string& meshFilePath, RGBColor _color, Point3f location) :
	GameObject(_name, meshFilePath, _color) 
{
	init( location);
}

CoinObject::CoinObject(const std::string& _name, const std::string& meshFilePath, const std::string& textureImageFilename, Point3f location) :
  GameObject(_name, meshFilePath, textureImageFilename) 
{ 
	init( location);
}

void CoinObject::init( Point3f location) 
{
	setScale( COIN_SCALE);
	setTranslation(location);
	setRotation(rand() % 360, ROTATION_AXIS);
}

void CoinObject::rotateCoin()
{
	rotate(COIN_ROTATION);
	if (0 > rotation)
	{
		rotate(360);
	}
}

void CoinObject::update()
{
	rotateCoin();
	
	float distance = distanceFrom( player);

	if ( COLLECT_DISTANCE > distance )
	{
		//got the coin!
		player->addCoin();
		_destroyed = true;

		if (name == "star") {
			player->win();
		}
	}
	
	return;
}

void CoinObject::collisionHandler(GameObject *gameObject, Collision collision) {
	return;
}

void CoinObject::generateCoins( Scene* scene)
{
	scene->addObject(GAME_SCREEN, new CoinObject("coin1", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_1_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin2", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_2_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin3", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_3_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin4", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_4_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin5", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_5_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin6", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_6_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin7", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_7_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin8", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_8_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin9", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_9_LOCATION));
	
	scene->addObject(GAME_SCREEN, new CoinObject("coin10", "objects/textured_coin.obj", "textures/Coin_texture.png", COIN_10_LOCATION));
	
	CoinObject* goal = new CoinObject("star", "objects/star.obj", RGBColor(1.0, 1.0, 0.0), STAR_LOCATION);
	goal->setScale( Point3f( 0.3, 0.3, 0.3));
	scene->addObject(GAME_SCREEN, goal);
}
