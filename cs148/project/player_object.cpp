#include "player_object.h"

#include "coin_object.h"
#include "scene.h"

extern GameObject* environment;
extern Scene* scene;

PlayerObject::PlayerObject(const std::string& _name, const std::string& meshFilePath, RGBColor _color) :
	GameObject(_name, meshFilePath, _color) 
{
	init();
}

PlayerObject::PlayerObject(const std::string& _name, const std::string& meshFilePath, const std::string& textureImageFilename) :
  GameObject(_name, meshFilePath, textureImageFilename) 
{ 
	init();
}

void PlayerObject::init() 
{
	lives = INITIAL_LIVES;
	coins = 0;
	resetPlayer();
}

void PlayerObject::resetPlayer() {
	setRotation(0, Point3f( 0.0, 1.0, 0.0));
	t = INITIAL_TRANSLATION;
	directionAngle = INITIAL_DIRECTION;
	setScale(PLAYER_SCALE);
	direction.y = GRAVITY;
	turn(0);
}

void PlayerObject::update() {
	Point3f lastT = t;

	t.x = t.x + (speed * direction.x);
	boost::optional<Collision> collisionX = getCollision(getPartitionToTriangles(false), environment->getPartitionToTriangles(true));
	if (collisionX.is_initialized()) {
		t.y = t.y + (SPEED * (direction.y + 1.2)); // try moving diagnoally up
		boost::optional<Collision> collisionX1 = getCollision(getPartitionToTriangles(false), environment->getPartitionToTriangles(true));
		if (collisionX1.is_initialized()) {
			t.x = lastT.x;
			t.y = lastT.y;
		}
	}

	lastT = t;

	t.z = t.z + (speed * direction.z);
	boost::optional<Collision> collisionZ = getCollision(getPartitionToTriangles(false), environment->getPartitionToTriangles(true));
	if (collisionZ.is_initialized()) {
		t.y = t.y + (SPEED * (direction.y + 1.2)); // try moving diagnoally up
		boost::optional<Collision> collisionZ1 = getCollision(getPartitionToTriangles(false), environment->getPartitionToTriangles(true));
		if (collisionZ1.is_initialized()) {
			t.z = lastT.z;
			t.y = lastT.y;
		}
	}

	lastT = t;

	t.y = t.y + (SPEED * direction.y); //now time for up and down
	boost::optional<Collision> collisionY = getCollision(getPartitionToTriangles(false), environment->getPartitionToTriangles(true));
	if (collisionY.is_initialized()) {
		t.y = lastT.y;
	}

	if (direction.y != 0) {
		direction.y += GRAVITY;
		if ( direction.y < MAX_Y_SPEED )
		{
			direction.y = MAX_Y_SPEED;
		}
	}

	speed = 0; //if speed were set, reset it to 0

	if (getBoundingBox().min.y < MIN_Y_THRESH) {
		die();
	}
}

void PlayerObject::jump()
{
	direction.y = JUMP_VELOCITY;
	//t.y += JUMP_VELOCITY;
	return;
}

void PlayerObject::step( float direction)
{
	speed = direction * SPEED;
	return;
}

void PlayerObject::turnLeft()
{
	turn( LEFT_TURN);
	return;
}

void PlayerObject::turnRight()
{
	turn( RIGHT_TURN);
	return;
}

Point3f PlayerObject::getDirection()
{
	return direction;
}

void PlayerObject::turn( float degrees)
{
	//update the directionAngle
	directionAngle += degrees;
	if ( 360 < directionAngle )
	{
		directionAngle -= 360;
	}
	else if ( 0 > directionAngle )
	{
		directionAngle += 360;
	}
	
	rotate( 57.295779513 * degrees);
	
	//now update the direction vector
	direction.x = sin( directionAngle);
	direction.z = cos( directionAngle);

	//y is updated as a part of jump/animate.

	return;
}

void PlayerObject::die()
{
	lives--;
	resetPlayer();

	if (lives == 0) {
		scene->screen = LOSE_SCREEN;
		lives = INITIAL_LIVES;
	}
	
	return;
}

void PlayerObject::win()
{
	lives = INITIAL_LIVES;
	resetPlayer();
	scene->screen = WIN_SCREEN;

	return;
}

void PlayerObject::addCoin()
{
	coins++;
	
	if ( coins == TOTAL_COINS )
	{
		//win();
	}
	
	return;
}
