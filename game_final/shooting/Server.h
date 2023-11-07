#pragma once
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <map>
#include <chrono>
#include <sstream>
#include <filesystem>
#include <functional>
#include "utils.h"
#include <deque>
#include <set>


struct OnlineClient {

	OnlineClient() {};
	OnlineClient(sf::IpAddress _ip, unsigned short _port, std::string _name, int _clientID, int _ts, int _challengeSol) :ip(_ip), port(_port), name(_name), clientID(_clientID), tsLastMessage(_ts), challengeSol(_challengeSol) 
	{	
		firstLetterNum = name[1] - 65; //Miramos el segundo caracter porque el primero es siempre un espacio		
	}

	int GetAddPcounter() {
		std::unique_lock<std::mutex> lockCounter(mutexPcounter);
		int temp = packetCounter;
		packetCounter++;
		lockCounter.unlock();
		return temp;
	}

	sf::IpAddress ip;
	unsigned short port;
	std::string name;
	int clientID, challengeSol, tsLastMessage, currentMatchId = -1, firstLetterNum;	
	int packetCounter = 0;
	std::mutex mutexPcounter;
};

struct OnlineMatch {
	int idMatch, namePlayer1Code;
	std::mutex playersDequeMutex;
	std::atomic<int>* playersInGame = nullptr;
	std::deque<OnlineClient*> playersInMatch;
	std::vector<std::pair<int, sf::Vector2f>> bulletsInMatch;

	~OnlineMatch() { 
		delete playersInGame; 
		playersInGame = nullptr; 
		playersInMatch.clear();	
	}
	OnlineMatch(int _idMatch, int _namePlayer1Code, OnlineClient* _player1) :idMatch(_idMatch), namePlayer1Code(_namePlayer1Code) { playersInGame = new std::atomic<int>(); playersInGame->store(1); playersInMatch.push_back(_player1); }
	bool Join(OnlineClient* _player2){ 
		try {
			std::unique_lock<std::mutex> lock(playersDequeMutex);
			if (playersInMatch.size() > 1) {
				lock.unlock();
				return false;
			}

			playersInMatch.push_back(_player2);
			lock.unlock();

			return true;
		}catch (const std::exception& e) { std::cout << "\nExcepción OM1 capturada: " << e.what() << std::endl; }

	}
	int PlayerQuit(int idPlayerLeft) {
		try {
			std::unique_lock<std::mutex> lock(playersDequeMutex);

			if (playersInMatch[0]->clientID == idPlayerLeft) {
				playersInMatch.pop_front();
			}
			else {
				playersInMatch.pop_back();
			}


			if (!playersInMatch.empty()) {
				playersInGame->store(1);
				namePlayer1Code = playersInMatch[0]->firstLetterNum;
				lock.unlock();
				return playersInMatch[0]->clientID;
			}
			else {
				playersInGame->store(0);
				lock.unlock();
				return -1;
			}
		}catch (const std::exception& e) { std::cout << "\nExcepción OM2 capturada: " << e.what() << std::endl; }
	}
};

struct ServerStats {
	sf::RenderWindow* window;
	bool isDragging = false;
	std::atomic<bool>* loop = nullptr;
	std::atomic<float>* _chancePacketLoss = nullptr;
	ThreadState* threadState;
	sf::RectangleShape* bar;
	sf::RectangleShape* pointer;
	std::chrono::system_clock::time_point lastRttRefresh;

	float rtt = 0, rttInScreen ,probabilityLosePaquet = 0;
	std::mutex mutexRtt, mutexPLP;

	~ServerStats() {		 
		delete bar;
		bar = nullptr;
		delete pointer;
		pointer = nullptr;		
		delete loop; 
		loop = nullptr; 
		delete _chancePacketLoss; 
		_chancePacketLoss = nullptr; 
		threadState = nullptr; 		
	}
	void Init() { 
		window = new sf::RenderWindow(); 
		window->create(sf::VideoMode(300, 100), "Server Stats"); 

		loop = new std::atomic<bool>();
		loop->store(true);

		rttInScreen = rtt;
		lastRttRefresh = std::chrono::system_clock::now();

		// Barra del slider
		bar = new sf::RectangleShape(sf::Vector2f(100, 10)); 
		bar->setPosition(180, 47);
		bar->setFillColor(sf::Color::Blue);
		
		// Puntero del slider
		pointer = new sf::RectangleShape(sf::Vector2f(10, 30)); 
		pointer->setOrigin(5, 5);
		pointer->setPosition(180, 42);
		pointer->setFillColor(sf::Color::Red);

		Update(); 
	}
	void Update(){

		*threadState = ThreadState::LOOPING;

		while (window->isOpen()) {
			if (!loop->load())
				break;
			sf::Event event;
			while (window->pollEvent(event)) {
				if (event.type == sf::Event::Closed) {
					loop->store(false);
					window->close();
					break;					
				}
			}

			if (event.type == sf::Event::MouseButtonPressed)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
				{
					sf::Vector2i mousePos = sf::Mouse::getPosition(*window);
					sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

					// Verificar si el mouse hizo clic en el puntero del slider
					if (pointer->getGlobalBounds().contains(mousePosF))
					{
						isDragging = true;
					}
				}
			}
			else if (event.type == sf::Event::MouseButtonReleased)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
				{
					isDragging = false;
				}
			}
			else if (event.type == sf::Event::MouseMoved)
			{
				if (isDragging)
				{
					sf::Vector2i mousePos = sf::Mouse::getPosition(*window);
					sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

					// Restringir la posición del puntero dentro de la barra
					if (mousePosF.x < 180)
						mousePosF.x = 180;
					else if (mousePosF.x > 280)
						mousePosF.x = 280;

					_chancePacketLoss->store(mousePosF.x - 180);

					pointer->setPosition(mousePosF.x, 42);
				}
			}

			//Ensenyar media de rtts actualizada cada 2 segundos
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - lastRttRefresh).count() >= 2000) {
				rttInScreen = rtt;
				lastRttRefresh = std::chrono::system_clock::now();
			}

			Render();

			std::chrono::milliseconds tiempoEspera(10);
			std::this_thread::sleep_for(tiempoEspera);
		}

		*threadState = ThreadState::ENDED;

	}
	void Render() {
		//Texto RTT

		sf::Text text;
		sf::Font font;

		std::filesystem::path directorio_actual = std::filesystem::current_path();
		std::string nombre_ultima_carpeta = directorio_actual.filename().string();
		
		std::string ruta_archivo;
		if (nombre_ultima_carpeta == "shooting") {	ruta_archivo = "resources/";}
		else { ruta_archivo = "../../shooting/resources/";}

		if (!font.loadFromFile(ruta_archivo + "fonts/courbd.ttf")) { std::cout << "Can't load the font file" << std::endl; return; }

		std::unique_lock<std::mutex> lock(mutexRtt);
		text = sf::Text("RTT: " + std::to_string(rttInScreen) + " ms\n\nChance packet loss:\n\n                      0%         100%", font, 14);
		lock.unlock();

		text.setFillColor(sf::Color(0, 0, 0));
		text.setStyle(sf::Text::Italic);
		text.setPosition(0, 10);				

		window->clear(sf::Color::White);
		window->draw(text);
		window->draw(*bar);
		window->draw(*pointer);
		window->display();

		bool isDragging = false;
	}
	void SetRtt(float _rtt){
		std::unique_lock<std::mutex> lock(mutexRtt);
		rtt = _rtt;
		lock.unlock();
	}
};

struct ServerPC {
	ServerPC(){}
	ServerPC(std::string _content, std::string _id, std::chrono::system_clock::time_point _ts) :content(_content), id(_id), tsFirstSend(_ts) , tsLastSend(_ts){}
	std::string content, id;	
	std::chrono::system_clock::time_point tsLastSend, tsFirstSend;	
};

struct PingPongData {
	int playerID;
	std::atomic<int>* currentContainer = nullptr, *timesSended = nullptr;	
	std::chrono::system_clock::time_point _ts;
	std::mutex mutexContainer, mutexTs;
	std::function<void(const int&, const std::string&, const int&, const bool&)> onSendPingPong;	

	PingPongData(){}
	PingPongData(int id, int container) :playerID(id) { 
		currentContainer = new std::atomic<int>(); 
		timesSended = new std::atomic<int>(); 
		currentContainer->store(container); 
		timesSended->store(0); 
		UpdateTs(); 		
	}
	~PingPongData() {
		delete currentContainer;
		currentContainer = nullptr;
		delete timesSended;
		timesSended = nullptr;
	}
	bool Update() {

		std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - GetTs());
		if ((timesSended->load() == 0 && diff.count() >= 10000) || (timesSended->load() > 0 && diff.count() >= 2000)) {
			if (timesSended->load() == 5) {
				return true;
			}
			else if (onSendPingPong) {
				int aux = timesSended->load() + 1;
				timesSended->store(aux);
				std::string messageToSend = std::string("_PING_ID") + std::to_string(playerID);
				std::cout << "PingSended\n";
				onSendPingPong(playerID, messageToSend, currentContainer->load(), false);
				UpdateTs();
			}
		}
		return false;	
	}
	void UpdateTs(){
		std::unique_lock<std::mutex> lock(mutexTs);
		_ts = std::chrono::system_clock::now();
		lock.unlock();
	}
	void SetSendPingToPlayer(const std::function<void(const int& playerId, const std::string& message, const int& containerNum, const bool & isPacketACK)>& callback) {
		onSendPingPong = callback;
	}	
	std::chrono::system_clock::time_point GetTs(){
		std::unique_lock<std::mutex> lock(mutexTs);
		std::chrono::system_clock::time_point aux = _ts;
		lock.unlock();
		return aux;
	}	
};

struct UpdateResponse {
public:
	int match = -1, id1 = -1, id2 = -1;
	std::vector<MoveData>p1Moves;	
	std::vector<MoveData>p2Moves;	
	void GenerateUpdate();
	std::string updateMessage;
	
};

class Server
{
//public:
//	Server(){}
public:
	Server(std::atomic<bool>* isSessionAcive);
	~Server();
	bool InitServer();
	void CloseServer();
private:
	std::atomic<bool>* sessionActive = nullptr;
	std::atomic<bool>* hasEntered = nullptr;
	std::atomic<bool>* canEnterAgain = nullptr;
	std::atomic<bool>* canCloseGame = nullptr;
//
	ServerStats serverStats;
//
	sf::UdpSocket* udpSocket;
	int idPlayers = 1, commandId = 0, idMatches = 0;
	float rtt = 0;
	std::atomic<float>*chancePacketLoss = nullptr;
//
	ThreadData mapThreadCleaner;
//
	std::deque<float>rttValues;
//
	std::multimap<std::pair<float, int>, MoveData> playerMoves; //Estara ordenado por tiempo y Id

	std::map<int, PingPongData*>clientPings;

	std::map<int, OnlineClient*> clientsOnlineIdClass;
	std::map <int, OnlineClient*> clientsSolvingChallenge;
//
	std::map<std::string, std::function<void(int, std::string)>> commandMap;
	std::map <std::string, ThreadData>serverThreadsData;
	std::map<std::string, ServerPC> criticalPackets;
	std::map<int, OnlineMatch*> activeMatches;
//	
	std::mutex mutexMapClientsSolvingChallenge, mutexMapClientsConnected, mutexIdPlayersVariable, mutexIdCommandVariable, mutexSendMssgFunction, 
		mutexThreadsData, mutexCPmap, mutexRttQueue, mutexOnConfirm, mutexExtra, mutexMatches, mutexIdMatches, mutexSend, mutexPings, mutexMoves;
//
	void ThreadsMapCleaner();
	void HandlePlayersMoves();
	void AddPlayerMove(int playerId, std::string message);
	void HandleClientsToServerMssg();
	void ExecuteCommand(std::string message, sf::IpAddress, unsigned short port, std::string commandThreadMapKey);
	void HandleServerToClientsMssg(int playerId, std::string message, int containerNum, bool isPacketACK = false);	
	void HandlePings();
	void PlayerStartgameType(int playerId, std::string message);
	void RemoveClient(int playerId, std::string message);
	void ConfirmCriticalPacket(std::string message);
	void HandleCriticalPacketsResend();
	void JoinMatch(int playerId);
	void NewMatch(int playerId);
	void ConfirmPong(int pID, std::string message);	
	std::pair<int, std::string> ReturnIdInsideMessage(std::string message, int containerNum);
};

