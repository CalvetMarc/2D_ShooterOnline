#pragma once
//#include "game.h"
#include "Client.h"
#include "Server.h"
#include <ctime> // Incluir la biblioteca para obtener semilla de tiempo
#include <cstdlib> // Incluir la biblioteca para usar std::rand
#include <atomic>

int main()
{   
    try {
        std::atomic<bool>* sessionActive;
        sessionActive = new std::atomic<bool>();
        sessionActive->store(true);

        Server* server = new Server(sessionActive);

        if (!server->InitServer()) { //Si la condicion se cumple usaremos esta consola para un cliente, sino para el servidor

            Client* client = new Client(sessionActive);
            client->ConnectToServer();

            do {} while (sessionActive->load());

            delete client;
            client = nullptr;
        }
        else {
            std::string closeServer;
            bool closingServer = false;

            do {
                if (!closingServer)
                    std::cin >> closeServer;
                if (closeServer == "exit") {
                    server->CloseServer();
                    closingServer = true;
                    closeServer = " ";
                }
            } while (sessionActive->load());
            std::cout << "ServerClosed\n";
        }
        delete server;
        server = nullptr;
        delete sessionActive;
        sessionActive = nullptr;
    }catch (const std::exception& e) { std::cout << "\nExcepción MAIN1 capturada: " << e.what() << std::endl; }

    return 0;
}