#include "Server.h"

const std::string LOCK_FILE = "server.lock";
#define SIZE 10.f

std::vector<std::string> challengeQuestions = {
    "1+5", "9-4", "4x4", "sqrt64", "3^2"
};

std::vector<int> challengeAnswers = {
    6, 5, 16, 8, 9
};

Server::Server(std::atomic<bool>* isSessionAcive)
{
    sessionActive = isSessionAcive;
    hasEntered = new std::atomic<bool>();
    hasEntered->store(false);
    canEnterAgain = new std::atomic<bool>();
    canEnterAgain->store(true);
    canCloseGame = new std::atomic<bool>();
    canCloseGame->store(false);

    commandMap["STARTORJOIN"] = [this](int pID, std::string message) {
        PlayerStartgameType(pID, message);
    };

    commandMap["ACK_"] = [this](int pID, std::string message) {
        ConfirmCriticalPacket(message);
    };

    commandMap["CLOSECLIENT"] = [this](int pID, std::string message) {
        RemoveClient(pID, message);
    };

    commandMap["PONG"] = [this](int pID, std::string message) {
        ConfirmPong(pID, message);
    };

    commandMap["MOVE_"] = [this](int pID, std::string message) {
        AddPlayerMove(pID, message);
    };
}

Server::~Server()
{ 
    
    delete hasEntered;
    hasEntered = nullptr;
    delete canEnterAgain;
    canEnterAgain = nullptr;
    delete canCloseGame;
    canCloseGame = nullptr;
    /*delete udpSocket;
    udpSocket = nullptr;*/        
    chancePacketLoss = nullptr;

    mutexMapClientsConnected.lock();
    try {
        for (auto it = clientsOnlineIdClass.begin(); it != clientsOnlineIdClass.end(); it++) {
            if (it == clientsOnlineIdClass.end())
                break;
            delete it->second;
            it->second = nullptr;
            if (it == clientsOnlineIdClass.end())
                break;
        }
    }catch(const std::exception& e){ std::cout << "\nExcepción 1S capturada: " << e.what() << std::endl; }

    clientsOnlineIdClass.clear();
    mutexMapClientsConnected.unlock();

    mutexMapClientsSolvingChallenge.lock();
    try {
        for (auto it = clientsSolvingChallenge.begin(); it != clientsSolvingChallenge.end(); it++) {
            if (it == clientsSolvingChallenge.end())
                break;
            delete it->second;
            it->second = nullptr;
            if (it == clientsSolvingChallenge.end())
                break;
        }
    }catch (const std::exception& e) { std::cout << "\nExcepción 2S capturada: " << e.what() << std::endl; }

    clientsSolvingChallenge.clear();
    mutexMapClientsSolvingChallenge.unlock();

    mutexPings.lock();

    try {
        for (auto it = clientPings.begin(); it != clientPings.end(); it++) {
            if (it == clientPings.end())
                break;
            delete it->second;
            it->second = nullptr;
            if (it == clientPings.end())
                break;
        }
    }
    catch (const std::exception& e) { std::cout << "\nExcepción 4s capturada: " << e.what() << std::endl; }

    mutexPings.unlock();

    mutexMatches.lock();
    try {
        for (auto it = activeMatches.begin(); it != activeMatches.end(); it++) {
            if (it == activeMatches.end())
                break;
            delete it->second;
            it->second = nullptr;
            if (it == activeMatches.end())
                break;
        }
    }catch (const std::exception& e) { std::cout << "\nExcepción 5s capturada: " << e.what() << std::endl; }

    activeMatches.clear();
    mutexMatches.unlock();
    
    sessionActive = nullptr;
}

//Funcion de encendido del servidor si esta apagado
bool Server::InitServer()
{
    std::filesystem::path directorio_actual = std::filesystem::current_path();
    std::string nombre_ultima_carpeta = directorio_actual.filename().string();

    //std::cout << nombre_ultima_carpeta;

    std::string ruta_archivo;
    if (nombre_ultima_carpeta == "shooting") {
        ruta_archivo = "../x64/Debug/" + LOCK_FILE;
    }
    else {
        ruta_archivo = directorio_actual.string() + "/" + LOCK_FILE;
    }

    // Comprobamos si ya existe el archivo de bloqueo
    std::ifstream lock(ruta_archivo);
    if (lock.is_open())
    {
        std::cout << "El servidor ya esta en ejecucion" << std::endl;
        return false;
    }

    // Creamos el archivo de bloqueo
    std::ofstream new_lock(ruta_archivo);

    // Creamos el socket y lo configuramos
    udpSocket = new sf::UdpSocket();
    udpSocket->setBlocking(true);
    if (udpSocket->bind(5000) != sf::Socket::Done) {
        std::cout << "Error listening on port 5000" << std::endl;
        return false;
    }

    chancePacketLoss = new std::atomic<float>;
    chancePacketLoss->store(0);
    serverStats._chancePacketLoss = chancePacketLoss;
    //serverStats = new ServerStats();
    //serverStats.Init();   

    std::cout << "Server is listening on port 5000" << std::endl;

    serverThreadsData.insert(std::make_pair("serverReceiveMessages", ThreadData()));
    serverThreadsData["serverReceiveMessages"].keepLoopingVar->store(true);
    serverThreadsData["serverReceiveMessages"].threadVar = new std::thread(&Server::HandleClientsToServerMssg, this);
    serverThreadsData["serverReceiveMessages"].threadVar->detach(); // No esperar a que termine el hilo

    serverThreadsData.insert(std::make_pair("cpResend", ThreadData()));
    serverThreadsData["cpResend"].keepLoopingVar->store(true);
    serverThreadsData["cpResend"].threadVar = new std::thread(&Server::HandleCriticalPacketsResend, this);
    serverThreadsData["cpResend"].threadVar->detach(); // No esperar a que termine el hilo

    serverThreadsData.insert(std::make_pair("pings", ThreadData()));
    serverThreadsData["pings"].keepLoopingVar->store(true);
    serverThreadsData["pings"].threadVar = new std::thread(&Server::HandlePings, this);
    serverThreadsData["pings"].threadVar->detach(); // No esperar a que termine el hilo

    serverThreadsData.insert(std::make_pair("moves", ThreadData()));
    serverThreadsData["moves"].keepLoopingVar->store(true);
    serverThreadsData["moves"].threadVar = new std::thread(&Server::HandlePlayersMoves, this);
    serverThreadsData["moves"].threadVar->detach(); // No esperar a que termine el hilo

    serverThreadsData.insert(std::make_pair("serverStats", ThreadData()));
    serverThreadsData["serverStats"].keepLoopingVar->store(true);
    serverStats.loop = serverThreadsData["serverStats"].keepLoopingVar;
    serverStats.threadState = &serverThreadsData["serverStats"].threadState;
    serverThreadsData["serverStats"].threadVar = new std::thread(&ServerStats::Init, &serverStats);
    serverThreadsData["serverStats"].threadVar->detach(); // No esperar a que termine el hilo

    mapThreadCleaner.threadVar = new std::thread(&Server::ThreadsMapCleaner, this);
    mapThreadCleaner.threadVar->detach();

    return true;
}

void Server::ThreadsMapCleaner()
{
    mapThreadCleaner.threadState = ThreadState::LOOPING;

    while (true) {

        std::unique_lock<std::mutex> lock(mutexThreadsData);
        bool keepLoop = mapThreadCleaner.keepLoopingVar->load() || serverThreadsData.size() > 0;
        if (!keepLoop) {
            lock.unlock();
            break;
        }
        try {
            for (auto it = serverThreadsData.begin(); it != serverThreadsData.end(); it++) {
                if (it == serverThreadsData.end())
                    break;

                if (it->second.threadState == ThreadState::ENDED) {
                    it = serverThreadsData.erase(it); // Obtenemos el siguiente iterador válido
                }
                //else {
                //    ++it; // Avanzamos al siguiente elemento del mapa
                //}
                if (it == serverThreadsData.end())
                    break;
            }
        }catch (const std::exception& e) { std::cout << "\nExcepción 4S capturada: " << e.what() << std::endl; }

        lock.unlock();
        std::chrono::milliseconds tiempoEspera(1000);
        std::this_thread::sleep_for(tiempoEspera);
    }

    mapThreadCleaner.threadState = ThreadState::ENDED;
    canCloseGame->store(true);
}

void Server::HandlePlayersMoves()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    serverThreadsData["moves"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    while (true) {
        lockThreads.lock();
        if (!serverThreadsData["moves"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();

        std::unique_lock<std::mutex> lockMoves(mutexMoves);

        std::map<int, UpdateResponse>gamesMoves;

        for(auto it = playerMoves.begin(); it != playerMoves.end(); ++it) { //Es recorren tots els moviments de tots els jugadors per ordenarlos tambe en partides. Estan ordenats per temps i ID
            
            if (gamesMoves.find(it->second.macthId) == gamesMoves.end()) { //Si no hi ha aquesta id de partida encara es crea
                gamesMoves[it->second.macthId].id1 = it->first.second; 

                std::unique_lock<std::mutex> lockMatches(mutexMatches);
                if (activeMatches[it->second.macthId]->playersInMatch.size() > 1) { // Si esta amb algu a la partida
                    if (activeMatches[it->second.macthId]->playersInMatch[0]->clientID == it->first.second)
                        gamesMoves[it->second.macthId].id2 = activeMatches[it->second.macthId]->playersInMatch[1]->clientID;
                    else if(activeMatches[it->second.macthId]->playersInMatch[1]->clientID == it->first.second)
                        gamesMoves[it->second.macthId].id2 = activeMatches[it->second.macthId]->playersInMatch[0]->clientID;
                }                
                else {
                    gamesMoves[it->second.macthId].id2 = -1;
                }
                lockMatches.unlock();
                gamesMoves[it->second.macthId].p1Moves.push_back(it->second);
            }
            else { //Si ya hi ha aquesta id de partida
                if (gamesMoves[it->second.macthId].id1 == it->first.second) { //Si el moviment es del player1 de la partida
                    gamesMoves[it->second.macthId].p1Moves.push_back(it->second);
                    gamesMoves[it->second.macthId].match = it->second.macthId;
                }
                else if(gamesMoves[it->second.macthId].id2 == it->first.second){//Si el moviment es del player2 de la partida
                    gamesMoves[it->second.macthId].id2 = it->first.second; 
                    gamesMoves[it->second.macthId].p2Moves.push_back(it->second);
                }
            }
        }       
        playerMoves.clear();
        for (auto it = gamesMoves.begin(); it != gamesMoves.end(); ++it) {
            it->second.GenerateUpdate();
            std::string mP1 = it->second.updateMessage + "_ID" + std::to_string(it->second.id1);
            HandleServerToClientsMssg(it->second.id1, mP1, 1, false);
            std::string mP2 = it->second.updateMessage + "_ID" + std::to_string(it->second.id2);
            HandleServerToClientsMssg(it->second.id2, mP2, 1, false);
            it->second.p1Moves.clear();
            it->second.p2Moves.clear();
        }
        gamesMoves.clear();
        

        lockMoves.unlock();

        std::chrono::milliseconds tiempoEspera(100);
        std::this_thread::sleep_for(tiempoEspera);
    }

    lockThreads.lock();
    serverThreadsData["moves"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Server::AddPlayerMove(int playerId, std::string message)
{  
    std::string messageCopy = message;
    size_t underscorePos = messageCopy.find('|');

    std::unique_lock<std::mutex> lockMoves(mutexMoves);
    while(underscorePos != std::string::npos) {
        std::string subMessage = messageCopy.substr(0, underscorePos);        
        MoveData mData(subMessage, SIZE, 0.2f, playerId);
        playerMoves.insert(std::make_pair(std::make_pair(mData.time, playerId), mData));
        messageCopy.erase(0, underscorePos + 1);
        underscorePos = messageCopy.find('|');
    }
    lockMoves.unlock();
    
}

//Funcion usada por un unico thread y revisar si alguien se conecta al servidor
void Server::HandleClientsToServerMssg()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    serverThreadsData["serverReceiveMessages"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    sf::Packet* p = new sf::Packet();
    sf::IpAddress* ipAdr = new sf::IpAddress();
    unsigned short* port = new unsigned short;

    while (true) {
        lockThreads.lock();
        if (!serverThreadsData["serverReceiveMessages"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();
        if (udpSocket->receive(*p, *ipAdr, *port) == sf::Socket::Done) {
            std::cout << "Message received at port: " << port << std::endl;       

            std::string message;
            *p >> message;

            std::cout << message << std::endl;

            std::unique_lock<std::mutex> lock(mutexIdCommandVariable);
            int currentIdCom = commandId;
            commandId++;
            lock.unlock();

            std::string commandKey = std::string("EC") + std::to_string(currentIdCom);
            lockThreads.lock();
            serverThreadsData.insert(std::make_pair(commandKey, ThreadData()));
            serverThreadsData[commandKey].keepLoopingVar->store(true);
            serverThreadsData[commandKey].threadVar = new std::thread(&Server::ExecuteCommand, this, message, *ipAdr, *port, commandKey);
            serverThreadsData[commandKey].threadVar->detach();
            lockThreads.unlock();

            delete p;
            p = new sf::Packet();
            delete port;
            port = new unsigned short;
            delete ipAdr;
            ipAdr = new sf::IpAddress();
        }
        else {
            std::cout << "error";
        }

    }

    lockThreads.lock();
    serverThreadsData["serverReceiveMessages"].threadState = ThreadState::ENDED;
    lockThreads.unlock();

}

void Server::ExecuteCommand(std::string message, sf::IpAddress ipA, unsigned short port, std::string commandThreadMapKey)
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    serverThreadsData[commandThreadMapKey].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    std::string messageCopy = message;
    int playerID = -1;

    std::string infoAfterPC = "";

    size_t foound = messageCopy.find(std::string("PC"));

    if (foound != std::string::npos) {

        messageCopy = messageCopy.erase(foound, std::string("PC").length());

        size_t underscorePos = messageCopy.find('_'); 
        if (underscorePos != std::string::npos) {

            infoAfterPC = messageCopy.substr(0 ,underscorePos);     
            messageCopy = messageCopy.erase(0,infoAfterPC.length() + std::string("_").length());            
        }
    }

    size_t found = messageCopy.find(std::string("HELLO_"));

    if (found != std::string::npos) { //Comprovem si es un client que es vol conectar


        messageCopy = messageCopy.erase(0, found + std::string("HELLO_").length());
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 4);
        int challengeRandomIndex = dis(gen);
        

        std::unique_lock<std::mutex> lock(mutexIdPlayersVariable);
        std::unique_lock<std::mutex> lock2(mutexMapClientsSolvingChallenge);
        try {
            int sol = challengeAnswers.at(challengeRandomIndex);
            OnlineClient* newClient = new OnlineClient(ipA, port, messageCopy, idPlayers, 0, sol);
            std::string keyS = std::to_string(idPlayers) + "-" + std::to_string(newClient->GetAddPcounter());

            std::string challengeToSend = challengeQuestions.at(challengeRandomIndex);
            std::string responseMessage = "PC" + keyS + std::string("_CHALLENGE") + challengeToSend + std::string("_");
            responseMessage += std::string("ID") + std::to_string(idPlayers);
            std::unique_lock<std::mutex> lockmcp(mutexCPmap);
            criticalPackets.insert(std::make_pair(keyS, ServerPC(responseMessage, keyS, std::chrono::system_clock::now())));
            lockmcp.unlock();

            clientsSolvingChallenge.insert(std::make_pair(idPlayers, newClient)); //Contenedor temporal mientras se resuelve el challenge
            lock2.unlock();        
            if(!infoAfterPC.empty())
                HandleServerToClientsMssg(idPlayers, std::string("ACK_") + infoAfterPC, 0, true);
            HandleServerToClientsMssg(idPlayers, responseMessage, 0);

            int aux = idPlayers;
            idPlayers++;
            lock.unlock();

            std::unique_lock<std::mutex> lockPings(mutexPings);            
            clientPings.emplace(aux, new PingPongData(aux, 0));
            clientPings[aux]->SetSendPingToPlayer([&](const int& pID, const std::string& message, const int& containerNum, const bool& isACK) {
                HandleServerToClientsMssg(pID, message, containerNum, isACK);
            });
            lockPings.unlock();

        }catch (const std::exception& e) { std::cout << "\nExcepción RS1 capturada: " << e.what() << std::endl; }

       
        


    }
    else {

        size_t found2 = messageCopy.find(std::string("CHALLENGE"));

        if (found2 != std::string::npos) { //Comprovem si es un client que esta responent al challenge

            std::pair<int, std::string> response = ReturnIdInsideMessage(messageCopy, 0);
            playerID = response.first;
            messageCopy = response.second;

            //Si no sha trobat cap id que coincideixi no fem res
            if (playerID != -1) {

                std::string responseMessage;

                messageCopy = messageCopy.erase(0, std::string("CHALLENGE").length());
                int clientSol = std::stoi(messageCopy);

                if (clientsSolvingChallenge[playerID]->challengeSol == clientSol) {

                    clientsOnlineIdClass.insert(std::make_pair(playerID, clientsSolvingChallenge[playerID]));
                    clientsSolvingChallenge.erase(playerID);

                    std::string keyS = std::to_string(playerID) + "-" + std::to_string(clientsOnlineIdClass[playerID]->GetAddPcounter());

                    responseMessage = "PC" + keyS + std::string("_STARTORJOIN_ID") + std::to_string(playerID);

                    std::unique_lock<std::mutex> lockmcp(mutexCPmap);
                    criticalPackets.insert(std::make_pair(keyS, ServerPC(responseMessage, keyS, std::chrono::system_clock::now())));
                    lockmcp.unlock();

                    if (!infoAfterPC.empty())
                        HandleServerToClientsMssg(playerID, std::string("ACK_") + infoAfterPC, 1, true);
                    HandleServerToClientsMssg(playerID, responseMessage, 1);

                    std::unique_lock<std::mutex> lockPings(mutexPings);                    
                    if (clientPings.find(playerID) != clientPings.end()) {
                        clientPings[playerID]->currentContainer->store(1);
                        clientPings[playerID]->timesSended->store(0);
                        clientPings[playerID]->UpdateTs();
                    }
                    lockPings.unlock();

                }
                else {
                    std::string keyS = std::to_string(playerID) + "-" + std::to_string(clientsSolvingChallenge[playerID]->GetAddPcounter());
                    responseMessage = "PC" + keyS + std::string("_CHALLFAILED_ID") + std::to_string(playerID);
                    std::unique_lock<std::mutex> lockmcp(mutexCPmap);
                    criticalPackets.insert(std::make_pair(keyS, ServerPC(responseMessage, keyS, std::chrono::system_clock::now())));
                    lockmcp.unlock();
                    
                    if (!infoAfterPC.empty())
                        HandleServerToClientsMssg(playerID, std::string("ACK_") + infoAfterPC, 0, true);
                    HandleServerToClientsMssg(playerID, responseMessage, 0);

                    std::unique_lock<std::mutex> lockPings(mutexPings);
                    if (clientPings.find(playerID) != clientPings.end()) {                        
                        clientPings[playerID]->timesSended->store(0);
                        clientPings[playerID]->UpdateTs();
                    }
                    lockPings.unlock();

                }
            }

        }
        else { //Es una accio diferent a les anteriors

            size_t founddd = messageCopy.find("ACK");

            if (founddd == std::string::npos) {

                std::pair<int, std::string> response = ReturnIdInsideMessage(messageCopy, -1);
                playerID = response.first;
                messageCopy = response.second;

            }
            else {
                size_t founddd2 = messageCopy.find("-");

                if (founddd2 != std::string::npos) {
                    playerID = std::stoi(messageCopy.substr(std::string("ACK").length() + 1, founddd2));
                }
            }

            

            if (!infoAfterPC.empty())
                HandleServerToClientsMssg(playerID, std::string("ACK_") + infoAfterPC, 1, true);

           

            //Si no sha trobat cap id que coincideixi no fem res
            if (playerID != -1) {

                std::unique_lock<std::mutex> lockPings(mutexPings);
                if (clientPings.find(playerID) != clientPings.end()) {
                    clientPings[playerID]->timesSended->store(0);
                    clientPings[playerID]->UpdateTs();
                }
                lockPings.unlock();

                if (!commandMap.empty()) {
                    try {
                        for (auto it = commandMap.begin(); it != commandMap.end(); it++) {
                            if (it == commandMap.end())
                                break;
                            std::string commandName = it->first;
                            size_t found = messageCopy.find(commandName);
                            if (found != std::string::npos) {
                                messageCopy = messageCopy.erase(0, found + commandName.length());
                                it->second(playerID, messageCopy);
                                break;
                            }
                            if (it == commandMap.end())
                                break;
                        }
                    }catch (const std::exception& e) { std::cout << "\nExcepción 5S capturada: " << e.what() << std::endl; }
                }
            }

        }

    }

    lockThreads.lock();
    serverThreadsData[commandThreadMapKey].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Server::HandleServerToClientsMssg(int playerId, std::string message, int containerNum, bool isPacketACK)
{    

    if (!isPacketACK) {

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 100);
        int challengeRandomIndex = dis(gen);

        if (challengeRandomIndex < chancePacketLoss->load()) {            
            return;
        }
    }

    sf::Packet p;
    p << message;

    std::unique_lock<std::mutex> lockSendMssg(mutexSendMssgFunction);

    if (containerNum == 1) {

        std::unique_lock<std::mutex> lockClientsConnected(mutexMapClientsConnected);
        if(clientsOnlineIdClass.find(playerId) != clientsOnlineIdClass.end())
        if (udpSocket->send(p, clientsOnlineIdClass[playerId]->ip, clientsOnlineIdClass[playerId]->port)) {
            lockSendMssg.unlock();
        }
        else {
            lockSendMssg.unlock();
        }
        lockClientsConnected.unlock();
    }
    else {

        std::unique_lock<std::mutex> lockClientsSolving(mutexMapClientsSolvingChallenge);
        if (clientsSolvingChallenge.find(playerId) != clientsSolvingChallenge.end())
        if (udpSocket->send(p, clientsSolvingChallenge[playerId]->ip, clientsSolvingChallenge[playerId]->port)) {
            lockSendMssg.unlock();
        }
        else {
            lockSendMssg.unlock();
        }
        lockClientsSolving.unlock();
    }

   
}

void Server::HandlePings()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    serverThreadsData["pings"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    std::vector<int>idPlayersToRemove;

    while (true) {

        lockThreads.lock();
        if (!serverThreadsData["cpResend"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();

        std::unique_lock<std::mutex> lockPings(mutexPings);

        for (auto it = clientPings.begin(); it != clientPings.end(); it++) {

            if (it == clientPings.end())
                break;
            
            if (it->second->Update())
                idPlayersToRemove.push_back(it->first);

            if (it == clientPings.end())
                break;            
        }

        for (int i = 0; i < idPlayersToRemove.size(); i++) {
            RemoveClient(idPlayersToRemove[i], " ");
            delete clientPings[idPlayersToRemove[i]];
            clientPings[idPlayersToRemove[i]] = nullptr;
            clientPings.erase(idPlayersToRemove[i]);
        }

        idPlayersToRemove.clear();

        lockPings.unlock();

        std::chrono::milliseconds tiempoEspera(50);
        std::this_thread::sleep_for(tiempoEspera);        
    }

    lockThreads.lock();
    serverThreadsData["pings"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Server::CloseServer()
{
    //Terminar aplicacion a los clientes
    

    std::unique_lock<std::mutex> lock(mutexMapClientsConnected);

    if (!clientsOnlineIdClass.empty()) {
        try {
            for (auto it = clientsOnlineIdClass.begin(); it != clientsOnlineIdClass.end(); it++) {

                if (it == clientsOnlineIdClass.end())
                    break;

                std::string keyS = std::to_string(it->first) + "-" + std::to_string(it->second->GetAddPcounter());
                std::string message = "PC" + keyS + std::string("_CLOSES_ID") + std::to_string(it->first);

                std::unique_lock<std::mutex> lockmcp(mutexCPmap);
                criticalPackets.insert(std::make_pair(keyS, ServerPC(message, keyS, std::chrono::system_clock::now())));
                lockmcp.unlock();

                lock.unlock();
                HandleServerToClientsMssg(it->first, message, 1, true);

                if (it == clientsOnlineIdClass.end())
                    break;
                lock.lock();
            }
        }catch (const std::exception& e) { std::cout << "\nExcepción 6S capturada: " << e.what() << std::endl; }
    }
    
    lock.unlock();
    

    //Tocar apartir daqui

    std::unique_lock<std::mutex> lock2(mutexMapClientsSolvingChallenge);

    if (!clientsSolvingChallenge.empty()) {
        try {
            for (auto it = clientsSolvingChallenge.begin(); it != clientsSolvingChallenge.end(); it++) {

                if (it == clientsSolvingChallenge.end())
                    break;

                std::string keyS = std::to_string(it->first) + "-" + std::to_string(it->second->GetAddPcounter());
                std::string message = "PC" + keyS + std::string("_CLOSES_ID") + std::to_string(it->first);

                std::unique_lock<std::mutex> lockmcp(mutexCPmap);
                criticalPackets.insert(std::make_pair(keyS, ServerPC(message, keyS, std::chrono::system_clock::now())));
                lockmcp.unlock();

                lock2.unlock();
                HandleServerToClientsMssg(it->first, message, 0, true);

                if (it == clientsSolvingChallenge.end())
                    break;
                lock2.lock();
            }
        }catch (const std::exception& e) { std::cout << "\nExcepción 7S capturada: " << e.what() << std::endl; }
    }
    
    lock2.unlock();    

    std::unique_lock<std::mutex> lockPings(mutexPings);

    for (auto it = clientPings.begin(); it != clientPings.end(); it++) {
        delete it->second;
        it->second = nullptr;
    }
    clientPings.clear();

    std::unique_lock<std::mutex> lockMatches(mutexMatches);

    for (auto it = activeMatches.begin(); it != activeMatches.end(); it++) {
        delete it->second;
        it->second = nullptr;
    }
    activeMatches.clear();

    lockMatches.unlock();

    std::unique_lock<std::mutex> lockThreadsEnd(mutexThreadsData);

    if (!serverThreadsData.empty()) {
        try {
            for (auto it = serverThreadsData.begin(); it != serverThreadsData.end(); it++) {
                if (it == serverThreadsData.end())
                    break;
                it->second.keepLoopingVar->store(false);
                if (it == serverThreadsData.end())
                    break;
            }

        }catch (const std::exception& e) { std::cout << "\nExcepción 8S capturada: " << e.what() << std::endl; }
    }
    serverStats.loop->store(false);
    lockThreadsEnd.unlock();
    mapThreadCleaner.keepLoopingVar->store(false);

    delete udpSocket;
    udpSocket = nullptr;

    do{
        std::chrono::milliseconds tiempoEspera(50);
        std::this_thread::sleep_for(tiempoEspera);

    } while (!canCloseGame->load());

    //Eliminar LOCK_FILE
    std::filesystem::path directorio_actual = std::filesystem::current_path();
    std::string nombre_ultima_carpeta = directorio_actual.filename().string();
    std::string ruta_archivo;
    if (nombre_ultima_carpeta == "shooting") {
        ruta_archivo = "../x64/Debug/" + LOCK_FILE;
    }
    else {
        ruta_archivo = directorio_actual.string() + "/" + LOCK_FILE;
    }
    if (std::filesystem::exists(ruta_archivo)) {
        std::filesystem::remove(ruta_archivo);
        std::cout << "Archivo eliminado correctamente.\n";
    }
    else {
        std::cout << "El archivo no se ha encontrado.\n";
    }

    //Terminar sesion
    sessionActive->store(false);
}

void Server::PlayerStartgameType(int playerId, std::string message)
{
    std::unique_lock<std::mutex> lockClientsConnected(mutexMapClientsConnected);
    std::cout << clientsOnlineIdClass[playerId]->name;
    lockClientsConnected.unlock();

    if (message == "J" || message == "j") {
        JoinMatch(playerId);
    }
    else if (message == "N" || message == "n") {
        NewMatch(playerId);
    }
}

void Server::RemoveClient(int playerId, std::string message)
{
    bool isSolving = false;

    std::unique_lock<std::mutex> locSC(mutexMapClientsSolvingChallenge);
    if (clientsSolvingChallenge.find(playerId) != clientsSolvingChallenge.end()) {

        std::string keyS = std::to_string(playerId) + "-" + std::to_string(clientsSolvingChallenge[playerId]->GetAddPcounter());
        std::string messageToSend2 = "PC" + keyS + std::string("_CLOSE_ID") + std::to_string(playerId);

        std::unique_lock<std::mutex> lockmcp2(mutexCPmap);
        criticalPackets.insert(std::make_pair(keyS, ServerPC(messageToSend2, keyS, std::chrono::system_clock::now())));
        lockmcp2.unlock();

        locSC.unlock();
        HandleServerToClientsMssg(playerId, messageToSend2, 0, true);
        locSC.lock();        

        delete clientsSolvingChallenge[playerId];
        clientsSolvingChallenge[playerId] = nullptr;
        clientsSolvingChallenge.erase(playerId);
        isSolving = true;
    }
    locSC.unlock();

    if (!isSolving) {
        std::unique_lock<std::mutex> lockCC(mutexMapClientsConnected);

        if (clientsOnlineIdClass.find(playerId) != clientsOnlineIdClass.end()) {            
            std::unique_lock<std::mutex> lockMat(mutexMatches);

            if (activeMatches.find(clientsOnlineIdClass[playerId]->currentMatchId) != activeMatches.end()) {
               int idPlayerNotifyAlone = activeMatches[clientsOnlineIdClass[playerId]->currentMatchId]->PlayerQuit(playerId);
               
               if (activeMatches[clientsOnlineIdClass[playerId]->currentMatchId]->playersInGame->load() == 0) {
                   delete activeMatches[clientsOnlineIdClass[playerId]->currentMatchId];
                   activeMatches[clientsOnlineIdClass[playerId]->currentMatchId] = nullptr;
                   activeMatches.erase(clientsOnlineIdClass[playerId]->currentMatchId);
               }
               lockMat.unlock();

               if (idPlayerNotifyAlone != -1) {        

                   std::string keyS = std::to_string(idPlayerNotifyAlone) + "-" + std::to_string(clientsOnlineIdClass[idPlayerNotifyAlone]->GetAddPcounter());
                   std::string messageToSend = "PC" + keyS + std::string("_NRS0_ID") + std::to_string(idPlayerNotifyAlone);

                   std::unique_lock<std::mutex> lockmcp(mutexCPmap);
                   criticalPackets.insert(std::make_pair(keyS, ServerPC(messageToSend, keyS, std::chrono::system_clock::now())));
                   lockmcp.unlock();

                   lockCC.unlock();
                   HandleServerToClientsMssg(idPlayerNotifyAlone, messageToSend, 1, true);
                   lockCC.lock();
               }
            }
            else
                lockMat.unlock();

            std::string keyS = std::to_string(playerId) + "-" + std::to_string(clientsOnlineIdClass[playerId]->GetAddPcounter());
            std::string messageToSend2 = "PC" + keyS + std::string("_CLOSE_ID") + std::to_string(playerId);

            std::unique_lock<std::mutex> lockmcp2(mutexCPmap);
            criticalPackets.insert(std::make_pair(keyS, ServerPC(messageToSend2, keyS, std::chrono::system_clock::now())));
            lockmcp2.unlock();

            lockCC.unlock();
            HandleServerToClientsMssg(playerId, messageToSend2, 1, true);
            lockCC.lock();

            delete clientsOnlineIdClass[playerId];
            clientsOnlineIdClass[playerId] = nullptr;
            clientsOnlineIdClass.erase(playerId);
        }        
        lockCC.unlock();
    }
    
}

void Server::ConfirmCriticalPacket(std::string message)
{     
    canEnterAgain->store(false);

    while (hasEntered->load() == true) {};

    std::unique_lock<std::mutex> lock(mutexCPmap);

    std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - criticalPackets[message].tsFirstSend);

    criticalPackets.erase(message);

    std::unique_lock<std::mutex> lock2(mutexRttQueue);
    if (rttValues.size() == 10)
        rttValues.pop_back();
    
    rttValues.push_front(diff.count());

    float tempRtt = 0;

    for each (float dff in rttValues)
    {
        tempRtt += dff;
    }

    tempRtt = tempRtt / rttValues.size();
    rtt = tempRtt;

    serverStats.SetRtt(rtt);
    lock2.unlock();

    lock.unlock();
    canEnterAgain->store(true);
        
}

void Server::HandleCriticalPacketsResend()
{
    

    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    serverThreadsData["cpResend"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    std::unique_lock<std::mutex> lockcpm(mutexCPmap);
    lockcpm.unlock();

    while (true) {
        
        lockThreads.lock();
        if (!serverThreadsData["cpResend"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();
                
        while (canEnterAgain->load() == false) {  };

        hasEntered->store(true);
        lockcpm.lock();
       
        try {
            for (auto it = criticalPackets.begin(); it != criticalPackets.end(); it++) {
                if (it == criticalPackets.end())
                    break;

                std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - it->second.tsLastSend);
                if (diff.count() >= 500) {

                    size_t found = it->first.find(std::string("-"));
                    std::string playerIDpc = it->first.substr(0, found);

                    size_t found2 = it->second.content.find(std::string("_STARTORJOIN_ID"));

                    if (found2 != std::string::npos) {
                        HandleServerToClientsMssg(std::stoi(playerIDpc), it->second.content, 1, false);
                    }
                    else {
                        HandleServerToClientsMssg(std::stoi(playerIDpc), it->second.content, 0, false);
                    }

                    it->second.tsLastSend = std::chrono::system_clock::now();

                }
                if (canEnterAgain->load() == false || it == criticalPackets.end())
                    break;
            }
        }catch (const std::exception& e) { std::cout << "\nExcepción 9S capturada: " << e.what() << std::endl; }
        
        lockcpm.unlock();
        hasEntered->store(false);

        
    }

    lockThreads.lock();
    serverThreadsData["cpResend"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Server::JoinMatch(int playerId)
{  
    std::unique_lock<std::mutex> lock(mutexMapClientsConnected);
    OnlineClient* p2Referene = clientsOnlineIdClass[playerId];
    lock.unlock();    

    bool gameAvailable = false;
    int idMatchNearestName = -1;
    int minCodeDistanceFound = -1;

    std::unique_lock<std::mutex> lock3(mutexMatches);
    try {
        for (auto it = activeMatches.begin(); it != activeMatches.end(); it++)
        {
            if (it->second->playersInGame->load() < 2) {

                if (it == activeMatches.end())
                    break;

                gameAvailable = true;

                if (idMatchNearestName == -1 && minCodeDistanceFound == -1) {
                    idMatchNearestName = it->first;
                    minCodeDistanceFound = std::abs(p2Referene->firstLetterNum - it->second->namePlayer1Code);
                }
                else if (std::abs(p2Referene->firstLetterNum - it->second->namePlayer1Code) < minCodeDistanceFound) {
                    idMatchNearestName = it->first;
                    minCodeDistanceFound = std::abs(p2Referene->firstLetterNum - it->second->namePlayer1Code);
                }

                if (it == activeMatches.end())
                    break;
            }
        }
    }catch (const std::exception& e) { std::cout << "\nExcepción 10S capturada: " << e.what() << std::endl; }

    if (gameAvailable) {
        if (activeMatches[idMatchNearestName]->Join(p2Referene)) {

            p2Referene->currentMatchId = idMatchNearestName;
            lock3.unlock();

            std::string keyS = std::to_string(playerId) + "-" + std::to_string(p2Referene->GetAddPcounter());
            std::string message = "PC" + keyS + std::string("_GS2-3_") + std::to_string(idMatchNearestName) + ("_ID") + std::to_string(playerId);

            std::unique_lock<std::mutex> lockmcp(mutexCPmap);
            criticalPackets.insert(std::make_pair(keyS, ServerPC(message, keyS, std::chrono::system_clock::now())));
            lockmcp.unlock();
            

            HandleServerToClientsMssg(playerId, message, 1);

            keyS = std::to_string(playerId) + "-" + std::to_string(p2Referene->GetAddPcounter());
            message = "PC" + keyS + std::string("_NRS1_ID") + std::to_string(playerId);

            lockmcp.lock();
            criticalPackets.insert(std::make_pair(keyS, ServerPC(message, keyS, std::chrono::system_clock::now())));
            lockmcp.unlock();

            HandleServerToClientsMssg(playerId, message, 1);
            
            lock3.lock();

            keyS = std::to_string(activeMatches[idMatchNearestName]->playersInMatch[0]->clientID) + "-" + std::to_string(activeMatches[idMatchNearestName]->playersInMatch[0]->GetAddPcounter());
            message = "PC" + keyS + std::string("_NRS1_ID") + std::to_string(activeMatches[idMatchNearestName]->playersInMatch[0]->clientID);

            lockmcp.lock();
            criticalPackets.insert(std::make_pair(keyS, ServerPC(message, keyS, std::chrono::system_clock::now())));
            lockmcp.unlock();

            HandleServerToClientsMssg(activeMatches[idMatchNearestName]->playersInMatch[0]->clientID, message, 1);

            lock3.unlock();
        }        
        else {
            lock3.unlock();
            NewMatch(playerId);
        }
    }
    else {
        lock3.unlock();
        NewMatch(playerId);
    }
    
}

void Server::NewMatch(int playerId)
{  
    std::unique_lock<std::mutex> lock2(mutexIdMatches);
    int tempId = idMatches;
    idMatches += 1;
    lock2.unlock();

    std::unique_lock<std::mutex> lock4(mutexMapClientsConnected);

    if (clientsOnlineIdClass.find(playerId) != clientsOnlineIdClass.end()) {
        std::unique_lock<std::mutex> lock3(mutexMatches);
        activeMatches.insert(std::make_pair(tempId, new OnlineMatch(tempId, clientsOnlineIdClass[playerId]->firstLetterNum, clientsOnlineIdClass[playerId])));
        clientsOnlineIdClass[playerId]->currentMatchId = tempId;
        lock3.unlock();

        std::string keyS = std::to_string(playerId) + "-" + std::to_string(clientsOnlineIdClass[playerId]->GetAddPcounter());
        lock4.unlock();

        std::string message = "PC" + keyS + std::string("_GS1-3_") + std::to_string(tempId) + ("_ID") + std::to_string(playerId);

        std::unique_lock<std::mutex> lockmcp(mutexCPmap);
        criticalPackets.insert(std::make_pair(keyS, ServerPC(message, keyS, std::chrono::system_clock::now())));
        lockmcp.unlock();        

        HandleServerToClientsMssg(playerId, message, 1);        
    }
    else {
        lock4.unlock();
    }

}

void Server::ConfirmPong(int pID, std::string message)
{
    std::unique_lock<std::mutex> lockPings(mutexPings);
    if (clientPings.find(pID) != clientPings.end()) {        
        clientPings[pID]->timesSended->store(0);
        clientPings[pID]->UpdateTs();
    }
    lockPings.unlock();
}

std::pair<int, std::string> Server::ReturnIdInsideMessage(std::string message, int containerNum)
{
    if (containerNum == -1) {

        std::unique_lock<std::mutex> lockClientsSolving(mutexMapClientsSolvingChallenge);
        if (!clientsSolvingChallenge.empty()) {
            try {
                for (auto it = clientsSolvingChallenge.begin(); it != clientsSolvingChallenge.end(); it++) { // Busquem si la id associada al missatge es d'algun client pendent de ser registrat
                    if (it == clientsSolvingChallenge.end())
                        break;
                    std::string playerIDinMessage = std::string("_ID") + std::to_string(it->first);
                    size_t found3 = message.find(playerIDinMessage);
                    if (found3 != std::string::npos) {
                        std::string messageCopy = message.erase(found3, found3 + playerIDinMessage.length());
                        lockClientsSolving.unlock();
                        return std::make_pair(it->first, messageCopy);
                    }
                    if (it == clientsSolvingChallenge.end())
                        break;
                }
            }
            catch (const std::exception& e) { std::cout << "\nExcepción 11S capturada: " << e.what() << std::endl; }
        }
        lockClientsSolving.unlock();
        std::unique_lock<std::mutex> lockClientsConnected(mutexMapClientsConnected);
        if (!clientsOnlineIdClass.empty()) {
            try {
                for (auto it = clientsOnlineIdClass.begin(); it != clientsOnlineIdClass.end(); it++) { // Busquem si la id associada al missatge es d'algun client registrat
                    if (it == clientsOnlineIdClass.end())
                        break;
                    std::string playerIDinMessage = std::string("_ID") + std::to_string(it->first);
                    size_t found4 = message.find(playerIDinMessage);
                    if (found4 != std::string::npos) {
                        std::string messageCopy = message.erase(found4, found4 + playerIDinMessage.length());
                        lockClientsConnected.unlock();
                        return std::make_pair(it->first, messageCopy);
                    }
                    if (it == clientsOnlineIdClass.end())
                        break;
                }
            }
            catch (const std::exception& e) { std::cout << "\nExcepción 12S capturada: " << e.what() << std::endl; }
        }
        lockClientsConnected.unlock();
    }
    else if (containerNum == 0) {

        std::unique_lock<std::mutex> lockClientsSolving(mutexMapClientsSolvingChallenge);
        if (!clientsSolvingChallenge.empty()) {
            try {
                for (auto it = clientsSolvingChallenge.begin(); it != clientsSolvingChallenge.end(); it++) { // Busquem si la id associada al missatge es d'algun client pendent de ser registrat
                    if (it == clientsSolvingChallenge.end())
                        break;
                    std::string playerIDinMessage = std::string("_ID") + std::to_string(it->first);
                    size_t found3 = message.find(playerIDinMessage);
                    if (found3 != std::string::npos) {
                        std::string messageCopy = message.erase(found3, found3 + playerIDinMessage.length());
                        lockClientsSolving.unlock();
                        return std::make_pair(it->first, messageCopy);
                    }
                    if (it == clientsSolvingChallenge.end())
                        break;
                }
            }catch (const std::exception& e) { std::cout << "\nExcepción 11S capturada: " << e.what() << std::endl; }
        }
        lockClientsSolving.unlock();

    }
    else if(containerNum == 1){

        std::unique_lock<std::mutex> lockClientsConnected(mutexMapClientsConnected);
        if (!clientsOnlineIdClass.empty()) {
            try {
                for (auto it = clientsOnlineIdClass.begin(); it != clientsOnlineIdClass.end(); it++) { // Busquem si la id associada al missatge es d'algun client registrat
                    if (it == clientsOnlineIdClass.end())
                        break;
                    std::string playerIDinMessage = std::string("_ID") + std::to_string(it->first);
                    size_t found4 = message.find(playerIDinMessage);
                    if (found4 != std::string::npos) {
                        std::string messageCopy = message.erase(found4, found4 + playerIDinMessage.length());
                        lockClientsConnected.unlock();
                        return std::make_pair(it->first, messageCopy);
                    }
                    if (it == clientsOnlineIdClass.end())
                        break;
                }
            }catch (const std::exception& e) { std::cout << "\nExcepción 12S capturada: " << e.what() << std::endl; }
        }
        lockClientsConnected.unlock();
    }

    return std::make_pair(-1, " ");
}

void UpdateResponse::GenerateUpdate()
{   

    sf::Vector2f currentPosP1;
    currentPosP1.x = -1000;
    currentPosP1.y = -1000;
    bool p1Poscorrect = true;
    float lastTimeCheckP1 = -1;

    sf::Vector2f currentPosP2;
    currentPosP2.x = -1000;
    currentPosP2.y = -1000;
    bool p2Poscorrect = true;
    float lastTimeCheckP2 = -1;

    

    std::map<int, AuxBullet> bulletsInGameFromP1;
    std::map<int, AuxBullet> bulletsInGameFromP2;

    
    for (int i = 0; i < p1Moves.size(); i++) { //Validem el moviment del jugador 1 i les seves bales
        if (!p1Moves[i].isBullet) { //Si lelement es el jugador
            if (p1Poscorrect) { //Guardem la posicio i la corregim si es necessari
                p1Poscorrect = p1Moves[i].MoveCorrectAndResponse();
                currentPosP1.x = p1Moves[i].pos.x;
                currentPosP1.y = p1Moves[i].pos.y;

            }
            else { //Si la posicio no es correcte no la guardarem
                p1Poscorrect = false;
            }            
        }
        else { //Si lelement es una bala
            if (p1Poscorrect) {  //Si el jugador no ha estat en cap posicio incorrecte              
                if (p1Moves[i].MoveCorrectAndResponse()) { //Si la posicio de la bala es correcte la actualitzem
                    bulletsInGameFromP1[p1Moves[i].bulletId].pos.x = p1Moves[i].pos.x;
                    bulletsInGameFromP1[p1Moves[i].bulletId].pos.y = p1Moves[i].pos.y;
                }
                else { //Si la posicio de la bala es incorrecte la marquem com que sha de borrar
                    bulletsInGameFromP1[p1Moves[i].bulletId].hasToDelete = true;
                }
                bulletsInGameFromP1[p1Moves[i].bulletId].idPlayer = p1Moves[i].pId;
                bulletsInGameFromP1[p1Moves[i].bulletId].angle = p1Moves[i].angle;
                bulletsInGameFromP1[p1Moves[i].bulletId].idBullet = p1Moves[i].bulletId;
            }
            else { //Si el jugador ha estat en una posicio incorrecte i la bala ha sigut creada despres la marquem com que sha de borrar per evitar trampes
                if (bulletsInGameFromP1.find(p1Moves[i].pId) == bulletsInGameFromP1.end()) {
                    bulletsInGameFromP1[p1Moves[i].bulletId].hasToDelete = true;
                    bulletsInGameFromP1[p1Moves[i].bulletId].idPlayer = p1Moves[i].pId;;
                    bulletsInGameFromP1[p1Moves[i].bulletId].angle = p1Moves[i].angle;
                    bulletsInGameFromP1[p1Moves[i].bulletId].idBullet = p1Moves[i].bulletId;
                }
            }
        }
        lastTimeCheckP1 = p1Moves[i].time;
    }

    for (int i = 0; i < p2Moves.size(); i++) { //Validem el moviment del jugador 2 i les seves bales
        if (!p2Moves[i].isBullet) { //Si lelement es el jugador
            if (p2Poscorrect) { //Si la nova posicio es correcte la guardem
                p2Poscorrect = p2Moves[i].MoveCorrectAndResponse();
                currentPosP2.x = p2Moves[i].pos.x;
                currentPosP2.y = p2Moves[i].pos.y;
            }
            else { //Si la posicio no es correcte no la guardarem
                p2Poscorrect = false;
            }
        }
        else { //Si lelement es una bala
            if (p2Poscorrect) {  //Si el jugador no ha estat en cap posicio incorrecte              
                if (p2Moves[i].MoveCorrectAndResponse()) { //Si la posicio de la bala es correcte la actualitzem
                    bulletsInGameFromP2[p2Moves[i].bulletId].pos.x = p2Moves[i].pos.x;
                    bulletsInGameFromP2[p2Moves[i].bulletId].pos.y = p2Moves[i].pos.y;
                }
                else { //Si la posicio de la bala es incorrecte la marquem com que sha de borrar
                    bulletsInGameFromP2[p2Moves[i].bulletId].hasToDelete = true;
                }
                bulletsInGameFromP2[p2Moves[i].bulletId].idPlayer = p2Moves[i].pId;
                bulletsInGameFromP2[p2Moves[i].bulletId].angle = p2Moves[i].angle;
                bulletsInGameFromP1[p2Moves[i].bulletId].idBullet = p2Moves[i].bulletId;
            }
            else { //Si el jugador ha estat en una posicio incorrecte i la bala ha sigut creada despres la marquem com que sha de borrar per evitar trampes
                if (bulletsInGameFromP2.find(p2Moves[i].pId) == bulletsInGameFromP2.end()) {
                    bulletsInGameFromP2[p2Moves[i].bulletId].hasToDelete = true;
                    bulletsInGameFromP2[p2Moves[i].bulletId].idPlayer = p2Moves[i].pId;
                    bulletsInGameFromP2[p2Moves[i].bulletId].angle = p2Moves[i].angle;
                    bulletsInGameFromP1[p2Moves[i].bulletId].idBullet = p2Moves[i].bulletId;
                }
            }
        }
        lastTimeCheckP2 = p2Moves[i].time;
    }

    updateMessage.clear();

    bool addUpdate = true;

    if (!p1Moves.empty()) {
        updateMessage = "UPDATEC" + std::to_string(id1) + "_T" + std::to_string(lastTimeCheckP1) + "_A" + std::to_string(p1Poscorrect) + "_P" + std::to_string(currentPosP1.x) + "/" + std::to_string(currentPosP1.y) + "_";
        addUpdate = false;
    }
    
    if(!bulletsInGameFromP1.empty())
    for (int i = 0; i < bulletsInGameFromP1.size(); i++) {
        if (!bulletsInGameFromP1[i].hasToDelete && bulletsInGameFromP1[i].pos.x == 0 && bulletsInGameFromP1[i].pos.y == 0) {

        }
        else {
            updateMessage += "B" + std::to_string(bulletsInGameFromP1[i].idBullet) + "_K" + std::to_string(bulletsInGameFromP1[i].hasToDelete) + "_V" + std::to_string(bulletsInGameFromP1[i].pos.x) + "/" + std::to_string(bulletsInGameFromP1[i].pos.y)
                + "_N" + std::to_string(bulletsInGameFromP1[i].angle.x) + "/" + std::to_string(bulletsInGameFromP1[i].angle.y) + "_";
        }
    }

    if (!p2Moves.empty()) {
        if (addUpdate) {
            updateMessage += "UPDATE";
        }
        updateMessage += "C" + std::to_string(id2) + "_T" + std::to_string(lastTimeCheckP2) + "_A" + std::to_string(p2Poscorrect) + "_P" + std::to_string(currentPosP2.x) + "/" + std::to_string(currentPosP2.y) + "_";
    }
    
    if(!bulletsInGameFromP2.empty())
        for (int i = 0; i < bulletsInGameFromP2.size(); i++) {
            if (!bulletsInGameFromP2[i].hasToDelete && bulletsInGameFromP2[i].pos.x == 0 && bulletsInGameFromP2[i].pos.y == 0) {

            }
            else {
                updateMessage += "B" + std::to_string(bulletsInGameFromP2[i].idBullet) + "_K" + std::to_string(bulletsInGameFromP2[i].hasToDelete) + "_V" + std::to_string(bulletsInGameFromP2[i].pos.x) + "/" + std::to_string(bulletsInGameFromP2[i].pos.y)
                    + "_N" + std::to_string(bulletsInGameFromP2[i].angle.x) + "/" + std::to_string(bulletsInGameFromP2[i].angle.y) + "_";

            }
        }
    
    

    p1Moves.clear();
    p2Moves.clear();
    bulletsInGameFromP1.clear();
    bulletsInGameFromP2.clear();
}
