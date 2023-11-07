#include "entities.h"



Bullet::Bullet(sf::Vector2f _pos, sf::Vector2f _angle, int _id, sf::Color bCollor)
{
	id = _id;
	_pos.y++; _pos.x++;
	pos = _pos;
	angle = _angle;
	bullet.setRadius(5.f);
	bullet.setFillColor(bCollor);
	bullet.setPosition(pos*SIZE);
}

void Bullet::Move()
{
	std::unique_lock<std::mutex> lockPos(mutexPos);
	pos = pos + angle* maxSpeed;
	bullet.setPosition(pos*SIZE);
	lockPos.unlock();
}

//bool Bullet::OutOfBounds() {
//	if (pos.x*SIZE < 10 || pos.y*SIZE < 10 || pos.x*SIZE > 820 || pos.y*SIZE > 560)
//	{
//		return true;
//	}
//	return false;
//}

sf::Vector2f Bullet::GetPos()
{
	sf::Vector2f aux;
	std::unique_lock<std::mutex> lockPos(mutexPos);
	aux = pos;
	lockPos.unlock();
	return aux;
}
sf::CircleShape Bullet::GetShape()
{
	return bullet;
}





Character::Character(sf::Vector2f initPos, int spriteNum, std::string resourcesPath)
{
	pos = initPos;
	std::string path = resourcesPath + "pj" + std::to_string(spriteNum);

	if (spriteNum == 1)
		pos = sf::Vector2f(20, 30);
	else 
		pos = sf::Vector2f(60, 30);

	if (!characterTex.loadFromFile(path + ".png"))
	{
		std::cout << "Can't load avatar's texture" << std::endl;
	}
	sprite.setTexture(characterTex);
	sprite.setPosition(pos.x*SIZE, pos.y*SIZE);
}

sf::Vector2f Character::GetPos()
{
	sf::Vector2f aux;
	std::unique_lock<std::mutex> lockPos(mutexPos);
	aux = pos;
	lockPos.unlock();
	return aux;
}

sf::Sprite Character::GetSprite()
{
	return sprite;
}

void Character::Move(sf::Vector2f dir)
{
	sprite.setPosition((pos.x+dir.x)*SIZE, (pos.y + dir.y)*SIZE);
	SetPos(sf::Vector2f((pos.x + dir.x), (pos.y + dir.y)));
}

void Character::MoveTo(sf::Vector2f pos)
{
	sprite.setPosition(pos.x * SIZE, pos.y * SIZE);
	SetPos(sf::Vector2f(pos.x, pos.y));
}

void Character::SetPos(sf::Vector2f newPos)
{	
	std::unique_lock<std::mutex> lockPos(mutexPos);
	pos = newPos;
	lockPos.unlock();	
}

void Character::SetSprite(int spriteNum, std::string resourcesPath) {

	std::string path = resourcesPath + "pj" + std::to_string(spriteNum);

	if (!characterTex.loadFromFile(path + ".png"))
	{
		std::cout << "Can't load avatar's texture" << std::endl;
	}

	if (spriteNum == 1)
		pos = sf::Vector2f(20, 30);
	else
		pos = sf::Vector2f(60, 30);

	sprite.setTexture(characterTex);
	sprite.setPosition(pos.x * SIZE, pos.y * SIZE);

}

//bool Character::OutOfBounds() {
//	if (pos.x*SIZE < 10 || pos.y*SIZE < 10 || pos.x*SIZE > 820 || pos.y*SIZE > 560)
//	{
//		return true;
//	}
//	return false;
//}


bool Character::CheckShoot(Bullet b) {
	sf::Vector2f bpos = b.GetPos();
	if ((bpos.x >= pos.x && bpos.x <= pos.x + 2) 
		&& (bpos.y >= pos.y && bpos.y <= pos.y + 2)
		){
		std::cout << "shoot " << life << std::endl;
		life--;
		if (life <= 0) sprite.setRotation(90);
		return true;
	}
	return false;
}
