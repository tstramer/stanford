#include "enemy_object.h"
#include "player_object.h"

extern PlayerObject* player;

EnemyObject::EnemyObject(const std::string& _name, const std::string& meshFilePath, RGBColor _color, Point3f location) :
	GameObject(_name, meshFilePath, _color) 
{
	init(location);
	direction = Point3f( 0.0, 0.0, 0.0);
	speed = 0;
}

EnemyObject::EnemyObject(const std::string& _name, const std::string& meshFilePath, const std::string& textureImageFilename, Point3f location) :
  GameObject(_name, meshFilePath, textureImageFilename) 
{ 
	init(location);
	direction = Point3f( 0.0, 0.0, 0.0);
	speed = 0;
}

void EnemyObject::init( Point3f location) 
{
	initialLocation = location;
	setScale( ENEMY_SCALE);
	setTranslation( location);
	setExplodeRadius( EXPLODE_DISTANCE);
}

void EnemyObject::update()
{
	float distance = distanceFrom( player);
	float horizontalD = horizontalDistanceFrom( player);
	float verticalD = verticalDistanceFrom( player);
	Point3f playerLoc = player->getPosition();

	if ( ATTACK_DISTANCE > distance && 1 > verticalD )
	{
		//put the enemy into attack mode!
		//dividing by distance approximately normalizes the result
		direction.x = (playerLoc.x - t.x) / distance;
		direction.y = (playerLoc.y - t.y) / distance;
		direction.z = (playerLoc.z - t.z) / distance;
		
		speed = ATTACK_SPEED;
		
		t.x = t.x + (speed * direction.x);
		t.z = t.z + (speed * direction.z);

		speed = 0; //if speed were set, reset it to 0
	}
	else
	{
		direction.x = 0.0;
		direction.y = 0.0;
		direction.z = 0.0;
	}
	
	//kill it, or explode?
	distance = distanceFrom( player);
	if ( explodeRadius > distance )
	{
		float horizontal = horizontalDistanceFrom( player);
		float vertical = verticalDistanceFrom( player);
		//more on top of it or more to the side?

		if ( horizontal <= (1.05 * vertical) )
		{
			//on top, bomb is disabled
			disable();
		}
		else
		{
			//on side! EXPLODE!!!
			explode();
		}
	}

	return;
}

void EnemyObject::disable()
{
	_destroyed = true;
	init(initialLocation);
	return;
}

void EnemyObject::explode()
{
	player->die();
	disable();
	return;
}

void EnemyObject::collisionHandler(GameObject *gameObject, Collision collision) {
	return;
}

void EnemyObject::setExplodeRadius( float _explodeRadius) { explodeRadius = _explodeRadius; }

void EnemyObject::generateEnemies( Scene* scene)
{
	scene->addObject(GAME_SCREEN, new EnemyObject("enemy1", "objects/bob_omb_new.obj", "textures/bob_omb_new.png", ENEMY_1_LOCATION));
	
	scene->addObject(GAME_SCREEN, new EnemyObject("enemy2", "objects/bob_omb_new.obj", "textures/bob_omb_new.png", ENEMY_2_LOCATION));
	
	scene->addObject(GAME_SCREEN, new EnemyObject("enemy3", "objects/bob_omb_new.obj", "textures/bob_omb_new.png", ENEMY_3_LOCATION));
	
	scene->addObject(GAME_SCREEN, new EnemyObject("enemy4", "objects/bob_omb_new.obj", "textures/bob_omb_new.png", ENEMY_4_LOCATION));
	
	EnemyObject* boss = new EnemyObject("enemy5", "objects/bob_omb_new.obj", "textures/bob_omb_new.png", BOSS_LOCATION);
	boss->setScale( Point3f( 0.25, 0.25, 0.25));
	boss->setExplodeRadius( 1.5);
	scene->addObject(GAME_SCREEN, boss);
}
