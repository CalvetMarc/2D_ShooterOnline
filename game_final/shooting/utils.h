#pragma once
#include <SFML/Graphics.hpp>
#include <SFML\Network.hpp>
#include <thread>
#include <atomic>
#include <random>
//#include <functional>

enum class Move { REST = 0 ,UP=1, DOWN=2, LEFT=3, RIGHT=4 };

enum class ThreadState { INACTIVE, LOOPING, ENDED };

enum class GameState{CONNECTING, CHALLENGING, PREGAME, PLAYING};

struct AuxBullet {
	int idBullet, idPlayer;
	sf::Vector2f pos, angle;
	bool hasToDelete = false;
};

struct MoveData {
	MoveData() {};
	MoveData(std::string info, float _size, float _speed, int _playerId);
	MoveData(int _matchId, int _playerId, int _isBullet, Move _move, sf::Vector2f _pos, float _time) : macthId(_matchId), pId(_playerId), isBullet(_isBullet), move(_move), pos(_pos), time(_time){};
	MoveData(int _matchId, int _playerId, int _isBullet, sf::Vector2f _angle, sf::Vector2f _pos, float _time, int _bulletId) : macthId(_matchId), pId(_playerId), isBullet(_isBullet), angle(_angle), pos(_pos), time(_time), bulletId(_bulletId) {};
	bool MoveCorrectAndResponse();
	std::string SerializeData();
	int macthId, pId, isBullet, bulletId = -1;
	Move move;
	sf::Vector2f pos, angle;
	float time, speed, size;
};

struct ThreadData {
	ThreadData() { 
		keepLoopingVar = new std::atomic<bool>(); 
		keepLoopingVar->store(true); 
	}

	std::atomic<bool>* keepLoopingVar = nullptr;
	ThreadState threadState = ThreadState::INACTIVE;
	std::thread* threadVar = nullptr;

	ThreadData::~ThreadData()
	{
		keepLoopingVar->store(false);

		if (threadState == ThreadState::LOOPING)
			do {} while (threadState != ThreadState::ENDED);

		if (threadState != ThreadState::INACTIVE)
			delete threadVar;
		threadVar = nullptr;
	}
};

std::string key2str(const sf::Keyboard::Key k); // A function to map sf::Keyboard to string