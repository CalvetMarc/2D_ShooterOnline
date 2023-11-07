#include "Client.h"


Client::~Client()
{
    delete game;
    game = nullptr;
   
    delete canCloseGame;
    canCloseGame = nullptr;
    text = nullptr;
    
    enunciadoText = nullptr;
    delete sendText;
    sendText = nullptr;
    delete gS;
    gS = nullptr;
    delete hasEntered;
    hasEntered = false;
    delete canEnterAgain;
    canEnterAgain = false;
    delete udpSocket;
    udpSocket = nullptr;
    
    chancePacketLoss = nullptr;
    
    sessionActive = nullptr;
}

Client::Client(std::atomic<bool>* isSessionAcive) :sessionActive(isSessionAcive)
{
    udpSocket = new sf::UdpSocket();
    udpSocket->setBlocking(true);    

    hasEntered = new std::atomic<bool>();
    hasEntered->store(false);
    canEnterAgain = new std::atomic<bool>();
    canEnterAgain->store(true);

    text = new sf::String("");
    enunciadoText = new sf::String("");
    sendText = new bool(false);

    chancePacketLoss = new std::atomic<float>();
    chancePacketLoss->store(0);

    canCloseGame = new std::atomic<bool>();
    canCloseGame->store(false);

    game = new Game(text, enunciadoText, sendText);
    game->_chancePacketLoss = chancePacketLoss;
    game->gS = gS;   
    game->SetCloseClientCallback([&](const std::string& message) {
        CloseClient(message);
    });

    commandMap["CHALLENGE"] = [this](std::string message) {
        SolveChallenge(message);
    };

    commandMap["CHALLFAILED"] = [this](std::string message) {
        ChallengeFailed(message);
    };

    commandMap["STARTORJOIN"] = [this](std::string message) {
        StartOrJoinGame(message);
    };

    commandMap["ACK_"] = [this](std::string message) {
        ConfirmCriticalPacket(message);
    };   
    
    commandMap["GS"] = [this](std::string message) {
        MatchStringToNewGameStatus(message);
    };

    commandMap["NRS"] = [this](std::string message) {
        SetRoomState(message);
    };
    
    commandMap["CLOSE"] = [this](std::string message) {
        CloseClient(message);
    }; 
    
    commandMap["PING"] = [this](std::string message) {
        ConfirmPing(message);
    };
    
    commandMap["UPDATE"] = [this](std::string message) {
        UpdatePositions(message);
    };
}

void Client::ConnectToServer()
{ 
    *enunciadoText = "- Type in your name to start playing";   

    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    clientThreadsData.insert(std::make_pair("receiveMssgs", ThreadData()));
    clientThreadsData["receiveMssgs"].keepLoopingVar->store(true);
    clientThreadsData["receiveMssgs"].threadVar = new std::thread(&Client::HandleClientTextReceived, this);
    clientThreadsData["receiveMssgs"].threadVar->detach();
    lockThreads.unlock();

    lockThreads.lock();
    clientThreadsData.insert(std::make_pair("getInputs", ThreadData()));
    clientThreadsData["getInputs"].keepLoopingVar->store(true);
    clientThreadsData["getInputs"].threadVar = new std::thread(&Client::GetGameText, this);
    clientThreadsData["getInputs"].threadVar->detach();
    lockThreads.unlock();

    lockThreads.lock();
    clientThreadsData.insert(std::make_pair("gameLoop", ThreadData()));
    clientThreadsData["gameLoop"].keepLoopingVar->store(true);
    game->tS = &clientThreadsData["gameLoop"].threadState;
    game->_keepPlaying = clientThreadCleaner.keepLoopingVar;
    clientThreadsData["gameLoop"].threadVar = new std::thread(&Game::run, game);
    clientThreadsData["gameLoop"].threadVar->detach();
    lockThreads.unlock();

    lockThreads.lock();
    clientThreadsData.insert(std::make_pair("cpResend", ThreadData()));
    clientThreadsData["cpResend"].keepLoopingVar->store(true);
    clientThreadsData["cpResend"].threadVar = new std::thread(&Client::HandleCriticalPacketsResend, this);
    clientThreadsData["cpResend"].threadVar->detach();
    lockThreads.unlock();

    lockThreads.lock();
    clientThreadsData.insert(std::make_pair("moves", ThreadData()));
    clientThreadsData["moves"].keepLoopingVar->store(true);
    clientThreadsData["moves"].threadVar = new std::thread(&Client::HandleSendMovesForValidation, this);
    clientThreadsData["moves"].threadVar->detach();
    lockThreads.unlock();

    clientThreadCleaner.threadVar = new std::thread(&Client::ThreadsMapCleaner, this);
    clientThreadCleaner.threadVar->detach();

}

void Client::GetGameText()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    clientThreadsData["getInputs"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    while (true) {

        std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
        if (!clientThreadsData["getInputs"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();

        if (*sendText) {
            /*if (*text == "exit") {
                CloseClient(" ");
                break;
            }*/

            if (*gS == GameState::CONNECTING) {

                std::unique_lock<std::mutex> lockcpid(mutexCPid);
                int tempPID = packetCounter;
                packetCounter += 1;
                lockcpid.unlock();

                std::string message = "PC" + std::to_string(tempPID) + "_HELLO_" + *text;

                auto ts = std::chrono::system_clock::now();
                std::unique_lock<std::mutex> lockcpm(mutexCPmap);
                criticalPackets.insert(std::make_pair(tempPID, ClientPC(message, tempPID, ts)));
                lockcpm.unlock();               
                

                HandleClientTextSend(message, false);                
                *sendText = false;
                text->clear();
            }
            else if (*gS == GameState::CHALLENGING) {

                std::unique_lock<std::mutex> lockcpid(mutexCPid);
                int tempPID = packetCounter;
                packetCounter += 1;
                lockcpid.unlock();

                std::string message = "PC" + std::to_string(tempPID) + "_CHALLENGE" + *text;

                auto ts = std::chrono::system_clock::now();
                std::unique_lock<std::mutex> lockcpm(mutexCPmap);
                criticalPackets.insert(std::make_pair(tempPID, ClientPC(message, tempPID, ts)));
                lockcpm.unlock();                

                HandleClientTextSend(message, true);
                *sendText = false;
                text->clear();
            }
            else if (*gS == GameState::PREGAME) {
                if (*text != "J" && *text != "j" && *text != "N" && *text != "n") {

                    size_t found = enunciadoText->find(std::string("Invalid"));

                    if (found == std::string::npos) {
                        *enunciadoText += ". Invalid character";
                        *sendText = false;
                        text->clear();
                    }

                }
                else {
                    std::unique_lock<std::mutex> lockcpid(mutexCPid);
                    int tempPID = packetCounter;
                    packetCounter += 1;
                    lockcpid.unlock();

                    std::string message = "PC"+ std::to_string(tempPID) + "_STARTORJOIN" + *text;

                    auto ts = std::chrono::system_clock::now();
                    std::unique_lock<std::mutex> lockcpm(mutexCPmap);
                    criticalPackets.insert(std::make_pair(tempPID, ClientPC(message, tempPID, ts)));
                    lockcpm.unlock();

                    HandleClientTextSend(message, true);
                    *sendText = false;
                    text->clear();
                }
            }
        }
        
    }
    
    lockThreads.lock();
    clientThreadsData["getInputs"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

bool Client::HandleClientTextSend(std::string message, bool addID = true, bool isACK)
{

    /*if (udpSocket == nullptr) {
        udpSocket = new sf::UdpSocket();
        udpSocket->setBlocking(true);
    }*/
    if (!isACK) {

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 101);
        int challengeRandomIndex = dis(gen);

        if (challengeRandomIndex < chancePacketLoss->load())
            return false;
    }

    sf::Packet p;

    if (addID)
        message += "_ID" + std::to_string(id);

    p << message;

    if (udpSocket->send(p, "127.0.0.1", 5000) == sf::Socket::Done)
    {
        /*std::string a;
        p >> a;
        std::cout << a << " mandado al servidor" << std::endl;*/

        return true;
    }
    else {
        std::cout << "Error al mandar mensaje" << std::endl;
        return false;
    }
}

void Client::HandleClientTextReceived()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    clientThreadsData["receiveMssgs"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    sf::Packet* p = new sf::Packet();
    sf::IpAddress* ipAdr = new sf::IpAddress();
    unsigned short* port = new unsigned short;

    std::chrono::milliseconds tiempoEspera(10);
    std::this_thread::sleep_for(tiempoEspera);

    while (true) {

        lockThreads.lock();
        if (!clientThreadsData["receiveMssgs"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();

        if (udpSocket->receive(*p, *ipAdr, *port) == sf::Socket::Done)
        {
            std::string message;
            *p >> message;
            std::cout << "Message from server received: " << message << std::endl;           

            std::unique_lock<std::mutex> lock(mutexIdCommandVariable);
            int currentIdCom = commandId;
            commandId++;
            lock.unlock();

            std::pair<bool, std::string>checkResults;

            size_t found = message.find(std::string("ACK_"));

            if (found == std::string::npos) {

                checkResults = CheckIfIdFitsMessage(message);                
            }          
            else {
                checkResults = std::make_pair(true, message);
            }

            if (checkResults.first) {
                std::string commandKey = std::string("EC") + std::to_string(currentIdCom);
                lockThreads.lock();
                clientThreadsData.insert(std::make_pair(commandKey, ThreadData()));
                clientThreadsData[commandKey].keepLoopingVar->store(true);
                clientThreadsData[commandKey].threadVar = new std::thread(&Client::ExecuteCommand, this, checkResults.second, *ipAdr, *port, commandKey);
                clientThreadsData[commandKey].threadVar->detach();
                lockThreads.unlock();

            }
            

            delete p;
            p = new sf::Packet();
            delete ipAdr;
            ipAdr = new sf::IpAddress();
            delete port;
            port = new unsigned short;

        }
        else {
            if(!name.empty())
                std::cout << "Error al recivir mensaje" << std::endl;
           
        }
    }

    lockThreads.lock();
    clientThreadsData["receiveMssgs"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Client::HandleSendMovesForValidation()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    clientThreadsData["moves"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    while (true) {        

        lockThreads.lock();
        if (!clientThreadsData["moves"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();

        if (*gS == GameState::PLAYING) {

            std::string messageMoves = "MOVE_";
            std::unique_lock<std::mutex> lock4(game->mutexGameElements);
            for (auto it = game->gameElements.begin(); it != game->gameElements.end(); ++it) {
                messageMoves += it->second.SerializeData() + "|";
            }
            lock4.unlock();

            if(messageMoves.size() > 5)
            HandleClientTextSend(messageMoves, true, false);
        }        

        std::chrono::milliseconds tiempoEspera(80);
        std::this_thread::sleep_for(tiempoEspera);
    }

    lockThreads.lock();
    clientThreadsData["moves"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Client::ThreadsMapCleaner()
{
    clientThreadCleaner.threadState = ThreadState::LOOPING;

    while (true) {

        std::unique_lock<std::mutex> lock(mutexThreadsData);
        bool keepLoop = clientThreadCleaner.keepLoopingVar->load() || clientThreadsData.size() > 0;
        if (!keepLoop) {
            lock.unlock();
            break;
        }
        if (!clientThreadCleaner.keepLoopingVar->load() && clientThreadsData.size() <= 1) {
            lock.unlock();
            break;
        }
        try {
            for (auto it = clientThreadsData.begin(); it != clientThreadsData.end(); it++) {
                if (it == clientThreadsData.end())
                    break;
                if (it->second.threadState == ThreadState::ENDED) {
                    it = clientThreadsData.erase(it); // Obtenemos el siguiente iterador válido
                }
                if (it == clientThreadsData.end())
                    break;
                //else {
                //    ++it; // Avanzamos al siguiente elemento del mapa
                //}
            }
        }catch (const std::exception& e) { std::cout << "\nExcepción 1P capturada: " << e.what() << std::endl; }
        lock.unlock();
        std::chrono::milliseconds tiempoEspera(1000);
        std::this_thread::sleep_for(tiempoEspera);
    }

    clientThreadCleaner.threadState = ThreadState::ENDED;
    canCloseGame->store(true);
}

void Client::CloseClient(std::string message)
{
    if (message == "S") {
        std::unique_lock<std::mutex> lockcpid(mutexCPid);
        int tempPID = packetCounter;
        packetCounter += 1;
        lockcpid.unlock();

        std::string messageTS = "PC" + std::to_string(tempPID) + "_CLOSECLIENT";

        auto ts = std::chrono::system_clock::now();
        std::unique_lock<std::mutex> lockcpm(mutexCPmap);
        criticalPackets.insert(std::make_pair(tempPID, ClientPC(messageTS, tempPID, ts)));
        lockcpm.unlock();

        HandleClientTextSend(messageTS, true, true);
    }

    std::unique_lock<std::mutex> lockThreadsEnd(mutexThreadsData);

    try {
        for (auto it = clientThreadsData.begin(); it != clientThreadsData.end(); it++) {
            if (it == clientThreadsData.end())
                break;
            it->second.keepLoopingVar->store(false);
            if (it == clientThreadsData.end())
                break;
        }
    }catch (const std::exception& e) { std::cout << "\nExcepción 2P capturada: " << e.what() << std::endl; }

    lockThreadsEnd.unlock();    
    udpSocket->setBlocking(false);
    udpSocket->unbind();
    clientThreadCleaner.keepLoopingVar->store(false);
    do {
        std::chrono::milliseconds tiempoEspera(50);
        std::this_thread::sleep_for(tiempoEspera);

    } while (!canCloseGame->load());

    sessionActive->store(false);

}

void Client::ExecuteCommand(std::string message, sf::IpAddress, unsigned short port, std::string commandThreadMapKey)
{
    //std::unique_lock<std::mutex> lock(mutexCommand);
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    clientThreadsData[commandThreadMapKey].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    std::string messageCopy = message;

    std::string infoAfterPC = "";

    size_t foound = messageCopy.find(std::string("PC"));

    if (foound != std::string::npos) {

        messageCopy = messageCopy.erase(foound, std::string("PC").length());

        size_t underscorePos = messageCopy.find('_');
        if (underscorePos != std::string::npos) {

            infoAfterPC = messageCopy.substr(0, underscorePos);
            messageCopy = messageCopy.erase(0, infoAfterPC.length() + std::string("_").length());
        }
    }

    if(!infoAfterPC.empty())
        HandleClientTextSend(std::string("ACK_") + infoAfterPC, false, true);

    if (!commandMap.empty()) {
        try {
            for (auto it = commandMap.begin(); it != commandMap.end(); it++) {
                if (it == commandMap.end())
                    break;
                std::string commandName = it->first;
                size_t found = messageCopy.find(commandName);
                if (found != std::string::npos) {
                    messageCopy = messageCopy.erase(0, found + commandName.length());
                    it->second(messageCopy);

                    break;
                }
                if (it == commandMap.end())
                    break;
            }
        }catch (const std::exception& e) { 
            std::cout << "\nExcepción 3P capturada: " << e.what() << std::endl; 
        }
    }
    //lock.unlock();
    lockThreads.lock();
    clientThreadsData[commandThreadMapKey].threadState = ThreadState::ENDED;
    lockThreads.unlock();

}

void Client::SolveChallenge(std::string message)
{   
    *enunciadoText = "- Solve the following challenge: " + message;
    *gS = GameState::CHALLENGING;   
}

void Client::ChallengeFailed(std::string message)
{
    size_t found = enunciadoText->find(std::string("Incorrect"));

    if (found == std::string::npos) {

        *enunciadoText += ".\n  Incorrect, try again";

    }    
    
}

void Client::StartOrJoinGame(std::string message)
{
    *gS = GameState::PREGAME;

    *enunciadoText = "- Challenge completed,\n  'J' to join game / 'N' to start new one";    
}

void Client::ConfirmCriticalPacket(std::string message)
{
    canEnterAgain->store(false);
    while (hasEntered->load() == true) {};
    std::unique_lock<std::mutex> lock(mutexCPmap);
    criticalPackets.erase(std::stoi(message));
    lock.unlock();

    canEnterAgain->store(true);
    //cvPaquetesCriticos.notify_one();
}

void Client::HandleCriticalPacketsResend()
{
    std::unique_lock<std::mutex> lockThreads(mutexThreadsData);
    clientThreadsData["cpResend"].threadState = ThreadState::LOOPING;
    lockThreads.unlock();

    std::unique_lock<std::mutex> lockcpm(mutexCPmap);
    lockcpm.unlock();

    while (true) {
        lockThreads.lock();
        if (!clientThreadsData["cpResend"].keepLoopingVar->load()) {
            lockThreads.unlock();
            break;
        }
        lockThreads.unlock();

        //cvPaquetesCriticos.wait(lockcpm, [this] { return canEnterAgain; });
        while (canEnterAgain->load() == false) { 
            /*if (!clientThreadCleaner.keepLoopingVar->load())
                break;
            std::chrono::milliseconds tiempoEspera(10);
            std::this_thread::sleep_for(tiempoEspera);*/
        };

        hasEntered->store(true);
        lockcpm.lock();
        try {
            for (auto it = criticalPackets.begin(); it != criticalPackets.end(); it++) {
                if (it == criticalPackets.end())
                    break;
                std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - it->second.ts);
                if (diff.count() >= 500) {

                    size_t found = enunciadoText->find(std::string("HELLO"));

                    if (found != std::string::npos) {
                        HandleClientTextSend(it->second.content, false, false);
                    }
                    else {
                        HandleClientTextSend(it->second.content, true, false);
                    }

                    it->second.ts = std::chrono::system_clock::now();

                }
                if (canEnterAgain->load() == false || it == criticalPackets.end())
                    break;
            }
        }catch (const std::exception& e) { std::cout << "\nExcepción 4P capturada: " << e.what() << std::endl; }
        lockcpm.unlock();
        hasEntered->store(false);
    }

    lockThreads.lock();
    clientThreadsData["cpResend"].threadState = ThreadState::ENDED;
    lockThreads.unlock();
}

void Client::ConfirmPing(std::string message)
{
    HandleClientTextSend("PONG", true, false);
}

void Client::UpdatePositions(std::string message){   
    std::string adw = message;
    try {
        while (!message.empty()) {
            std::string adadwdw = message;
            int idInfo = -1;
            try {
                size_t endClient = message.find(std::string("_"));
                idInfo = std::stoi(message.substr(1, endClient - 1));
                message.erase(0, endClient + 1);
            }
            catch (const std::exception& e) {
                std::cout << "\nExcepción 1r " << e.what() << std::endl;
            }
            
            if (idInfo == id) { //Es la info del jugador local

                size_t endTime = message.find(std::string("_"));
                float time = std::stof(message.substr(1, endTime - 1));
                message.erase(0, endTime + 1);

                std::unique_lock<std::mutex> lock4(game->mutexGameElements); //Borrar moviments ya revisats
                auto it = game->gameElements.upper_bound(time);
                game->gameElements.erase(game->gameElements.begin(), it);//--it
                lock4.unlock();

                size_t endPosAccepted = message.find(std::string("_"));
                bool accepted = (bool)std::stoi(message.substr(1, endPosAccepted - 1));
                message.erase(0, endPosAccepted + 1);

                size_t endPosX = message.find(std::string("/"));
                float posX = std::stof(message.substr(1, endPosX - 1));
                message.erase(0, endPosX + 1);
                size_t endPosY = message.find(std::string("_"));
                float posY = std::stof(message.substr(0, endPosY - 1));
                message.erase(0, endPosY + 1);

                if (!accepted && posX > 0 && posY > 0)
                    game->character->SetPos(sf::Vector2f(posX, posY)); //Ara poden venir bales o seguent jugador

                if (message[0] == 'B')  // Venen bales
                    while (!message.empty() && message[0] == 'B')
                    {
                        size_t endBID = message.find(std::string("_"));
                        int idBullet = std::stoi(message.substr(1, endBID - 1));
                        message.erase(0, endBID + 1);

                        size_t endDelete = message.find(std::string("_"));
                        bool hasToDelete = (bool)std::stoi(message.substr(1, endDelete - 1));
                        message.erase(0, endDelete + 1);

                        size_t endBPX = message.find(std::string("/"));
                        float bpx = std::stof(message.substr(1, endBPX - 1));
                        message.erase(0, endBPX + 1);
                        float bpy;
                        size_t endBPY = message.find(std::string("_"));
                        bpy = std::stof(message.substr(0, endBPY - 1));
                        message.erase(0, endBPY + 1);

                        size_t endAngleX = message.find(std::string("/"));
                        float angleX = std::stof(message.substr(1, endAngleX - 1));
                        message.erase(0, endAngleX + 1);
                        float angleY;
                        size_t endAngleY = message.find(std::string("_"));
                        angleY = std::stof(message.substr(0, endAngleY - 1));
                        message.erase(0, endAngleY + 1);

                        std::unique_lock<std::mutex> lockBullets(game->mutexBullets);
                        if (hasToDelete && (game->bullets.find(idBullet) != game->bullets.end())) {
                            delete game->bullets[idBullet];
                            game->bullets[idBullet] = nullptr;
                            game->bullets.erase(idBullet);
                        }
                        lockBullets.unlock();
                    }
            }
            else { //Jugador remoto

                size_t endTime = message.find(std::string("_"));
                float time = std::stof(message.substr(1, endTime - 1));
                message.erase(0, endTime + 1);

                size_t endPosAccepted = message.find(std::string("_"));
                bool accepted = (bool)std::stoi(message.substr(1, endPosAccepted - 1));
                message.erase(0, endPosAccepted + 1);

                if (game->character2 != nullptr) {

                    size_t endPosX = message.find(std::string("/"));
                    float posX = std::stof(message.substr(1, endPosX - 1));
                    message.erase(0, endPosX + 1);
                    size_t endPosY = message.find(std::string("_"));
                    float posY = std::stof(message.substr(0, endPosY - 1));
                    message.erase(0, endPosY + 1);

                    if(posX > 0 && posY > 0)
                    game->character2->MoveTo(sf::Vector2f(posX, posY)); //Ara poden venir bales o seguent jugador            

                    if (!message.empty() && message[0] == 'B')
                        while (!message.empty() && message[0] == 'B')
                        {
                            size_t endBID = message.find(std::string("_"));
                            int idBullet = std::stoi(message.substr(1, endBID - 1));
                            message.erase(0, endBID + 1);

                            size_t endDelete = message.find(std::string("_"));
                            bool hasToDelete = (bool)std::stoi(message.substr(1, endDelete - 1));
                            message.erase(0, endDelete + 1);

                            size_t endBPX = message.find(std::string("/"));
                            float bpx = std::stof(message.substr(1, endBPX - 1));
                            message.erase(0, endBPX + 1);
                            float bpy;
                            size_t endBPY = message.find(std::string("_"));
                            bpy = std::stof(message.substr(0, endBPY - 1));
                            message.erase(0, endBPY + 1);

                            size_t endAngleX = message.find(std::string("/"));
                            float angleX = std::stof(message.substr(1, endAngleX - 1));
                            message.erase(0, endAngleX + 1);
                            float angleY;
                            size_t endAngleY = message.find(std::string("_"));
                            angleY = std::stof(message.substr(0, endAngleY - 1));
                            message.erase(0, endAngleY + 1);

                            std::unique_lock<std::mutex> lockBullets2(game->mutexBullets2);
                            if (game->bullets2.find(idBullet) == game->bullets2.end()) { //La bullet es nova
                                if (!hasToDelete) {
                                    sf::Vector2f pos;
                                    pos.x = bpx;
                                    pos.y = bpy;
                                    sf::Vector2f angle;
                                    angle.x = angleX;
                                    angle.y = angleY;
                                    Bullet* b = new Bullet(pos, angle, idBullet, sf::Color::Red);
                                    game->bullets2[b->GetId()] = b;
                                }
                            }
                            else if (hasToDelete) {
                                delete game->bullets2[idBullet];
                                game->bullets2[idBullet] = nullptr;
                                game->bullets2.erase(idBullet);
                            }
                            lockBullets2.unlock();
                        }

                }
            }
        }

    } catch (const std::exception& e) { 
        std::cout << "\nExcepción UpdatePos: " << e.what() << std::endl; 
    }
    
}

void Client::MatchStringToNewGameStatus(std::string message)
{
    size_t found = message.find(std::string("-"));
    size_t foundEnd = message.find(std::string("_"));
    std::string enterOrder = message.substr(0, found);

    if (std::stoi(enterOrder) == 2)
        game->created = false;

    *gS = static_cast<GameState>(std::stoi(message.substr(found + 1, foundEnd - found - 1)));
    game->idGame = std::stoi(message.substr(foundEnd + 1));
    game->idPlayer = id;
}

void Client::SetRoomState(std::string message)
{
    bool isFull = std::stoi(message);
    game->roomFull = isFull;
    if (!isFull) {
        delete game->character2;
        game->character2 = nullptr;
    }
   
}

std::pair<bool, std::string> Client::CheckIfIdFitsMessage(std::string message)
{
    std::string messageCopy = message;
    size_t found2 = messageCopy.find(std::string("_ID"));

    if (found2 != std::string::npos) {
        size_t startPos = found2 + std::string("_ID").length();
        std::string subString = messageCopy.substr(startPos);
        if (id != -1) {                                              //Si teniem id assignada mirem si coincideix
            if (std::stoi(subString) == id) {
                std::string subString2 = messageCopy.substr(0, found2);
                return std::make_pair(true, subString2);
            }
        }
        else {                                                       //Si no teniem id assignada ens la quedem
            id = std::stoi(subString);
            std::string subString2 = messageCopy.substr(0, found2);
            return std::make_pair(true, subString2);
        }

    }
    else {
        size_t found3 = messageCopy.find(std::string("ACK_"));

        if (found3 != std::string::npos) {
            return std::make_pair(true, messageCopy);
        }
    }
    return std::make_pair(false, " ");
}





