#pragma once
#include <iostream>
#include <functional>
#include <string>
#include <mutex>
#include <thread>
#include <sstream>
#include <future>
#include "utils.h"
#include "game.h"

struct ClientPC {
	ClientPC();
	ClientPC(std::string _content, int _packetID, std::chrono::system_clock::time_point _ts) :content(_content), packetID(_packetID), ts(_ts) {}
	std::string content;
	int packetID;
	std::chrono::system_clock::time_point ts;
};

class Client
{
public:
	Client(std::atomic<bool>* isSessionAcive);
	~Client();
	void ConnectToServer();
private:
	Game* game;
	sf::String* text, *enunciadoText;
	bool* sendText;
	GameState* gS = new GameState(GameState::CONNECTING);
	int id = -1, commandId = 0, packetCounter = 0;
	std::string name, inputText;

	bool canTypeMessages;
	std::atomic<bool>* canCloseGame = nullptr;
	std::atomic<bool>*sessionActive = nullptr;
	std::atomic<bool>* hasEntered = nullptr;/*= false;*/
	std::atomic<bool>* canEnterAgain = nullptr;/* = true;*/

	sf::UdpSocket* udpSocket;
	ThreadData clientThreadCleaner;

	std::atomic<float>* chancePacketLoss = nullptr;

	std::mutex mutexSendMessage, mutexThreadsData, mutexIdCommandVariable, mutexCPmap, mutexCPid, mutexCommand;
	std::map <std::string, ThreadData>clientThreadsData;
	std::map<std::string, std::function<void(std::string)>> commandMap;
	std::map<int, ClientPC>criticalPackets;

	void GetGameText();	
	bool HandleClientTextSend(std::string message, bool addID, bool isACK = false);
	void HandleClientTextReceived();	
	void HandleSendMovesForValidation();
	void ExecuteCommand(std::string message, sf::IpAddress, unsigned short port, std::string commandThreadMapKey);
	void ThreadsMapCleaner();
	void CloseClient(std::string message);
	void SolveChallenge(std::string message);
	void ChallengeFailed(std::string message);
	void StartOrJoinGame(std::string message);
	void ConfirmCriticalPacket(std::string message);	
	void MatchStringToNewGameStatus(std::string message);
	void SetRoomState(std::string message);
	void HandleCriticalPacketsResend();	
	void ConfirmPing(std::string message);
	void UpdatePositions(std::string message);
	std::pair<bool, std::string> CheckIfIdFitsMessage(std::string message);
};

