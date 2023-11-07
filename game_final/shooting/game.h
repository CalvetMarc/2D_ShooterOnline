#pragma once
#include "entities.h"
#include "utils.h"
#include <thread>
#include <chrono>
#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <mutex>
#include <filesystem>
#include <future>

#define SIZE 10.f

class Game
{	
	std::string ruta_archivo;

	sf::String* inputtext, *enunciadoText;
	bool* sendText;

	// Threading variables
	std::mutex textInfoMutex;
	// GUI VARIABLES
	sf::RenderWindow window;
	sf::Texture characterTex;
	sf::Texture bg;
	sf::Sprite spriteBackground;
	//sf::String message;
	sf::Text text;
	sf::Text nameText;
	sf::Font font;
	sf::Event event;
	sf::RectangleShape nameRectangle;

	sf::Text textSlider;
	sf::RectangleShape* bar;
	sf::RectangleShape* pointer;

	// GAME VARIABLES
	bool playing = false;
	bool shooting = false;
	bool isDragging = false;

	sf::Vector2f cDir;

	std::chrono::high_resolution_clock::time_point startPlaying;

	//Intern Game Funcs
	void setUp();      // Initializing GUI
	void ChangeText(std::string message);	
	void GetInputActions();
	void GetInput();
	void DrawTextBox();
	void DrawBackGround();
	void DrawEntities();

public:
	std::map<int, Bullet*> bullets;  // Bullet container to manage them
	std::map<int, Bullet*> bullets2; // Bullet container to manage them
	GameState* gS;
	ThreadState* tS;
	std::atomic<float>* _chancePacketLoss = nullptr;
	std::atomic<bool>* _keepPlaying = nullptr;
	bool roomFull = false, created = true;
	std::function<void(const std::string&)> onCloseClient;
	Character* character;
	Character* character2;
	int idGame = -1, idPlayer = -1, idBullet = 0;
	std::multimap<float, MoveData>gameElements;
	std::mutex mutexGameElements, mutexBullets, mutexBullets2;

	Game(sf::String* _inputtext, sf::String* _enunciadoText, bool* _sendText);
	~Game();
	void run();        // Application loop
	void Render();	
	void SetCloseClientCallback(const std::function<void(const std::string&)>& callback) {
		onCloseClient = callback;
	}
};
