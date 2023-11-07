#pragma once
#include <iostream>
#include <fcntl.h>
//#include <vector>
#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#define SIZE 10.f
#include<mutex>

class Bullet {
	sf::Vector2f pos;
	float maxSpeed = .2;
	sf::Vector2f angle;  // Direction of the shoot
	sf::CircleShape bullet;
	int id;
	std::mutex mutexPos;
public:
	Bullet(sf::Vector2f initPos, sf::Vector2f angle, int _id, sf::Color bCollor);
	void Move(); // Set new position according to the angle
	//bool OutOfBounds();   // Returns true if bullet is out-of-bounds
	sf::Vector2f GetPos();
	int GetId() { return id; };
	sf::Vector2f GetAngle() { return angle; }; // No implemented
	void SetPos(sf::Vector2f newPos) { std::unique_lock<std::mutex> lockPos(mutexPos); pos = newPos; lockPos.unlock(); }; 
	sf::CircleShape GetShape();
};

class Character
{
	sf::Vector2f pos;
	sf::Texture characterTex;
	sf::Sprite sprite;
	int life = 100;  // When 0, the player dies
	std::mutex mutexPos;
public:
	Character(sf::Vector2f pos, int spriteNum, std::string resourcesPath);
	sf::Vector2f GetPos();
	sf::Sprite GetSprite();
	void SetPos(sf::Vector2f newPos);
	void SetLife(int l); // No implemented
	void SetSprite(int spriteNum, std::string resourcesPath);
	int GetLife();       // No implemented
	bool CheckShoot(Bullet b);   // Returns true if a bullet reaches the character
	//bool OutOfBounds();  // Returns true if character is out-of-bounds
	void Move(sf::Vector2f dir);
	void MoveTo(sf::Vector2f pos);
};

