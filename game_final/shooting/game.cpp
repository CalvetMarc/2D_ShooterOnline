#include "game.h"

void Game::ChangeText(std::string message)
{
	std::unique_lock<std::mutex> lock(textInfoMutex);
	text.setString(message);
	lock.unlock();
}

void Game::GetInputActions()
{
	if (event.key.code == sf::Keyboard::Escape) {
		/**inputtext = "exit";
		*sendText = true;*/
		_keepPlaying->store(false);
		if (onCloseClient) {
			onCloseClient("S");
		}
		return;
		//window.close();
	}

	if ((event.key.code == sf::Keyboard::Delete || event.key.code == sf::Keyboard::BackSpace) && inputtext->getSize() > 0) {
		if(!*sendText)
			inputtext->erase(inputtext->getSize() - 1, inputtext->getSize());
	}
	else if (event.key.code == sf::Keyboard::Return && inputtext->getSize() > 0 && !*sendText) {
		if (!*sendText)
			*sendText = true;		
	}
	else { 
		if (*gS != GameState::PLAYING) {

			if (!*sendText)
				*inputtext += key2str(event.key.code);

		}
		else {			
			cDir.x = 0;
			cDir.y = 0;
			
			std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startPlaying);
			float tiempoActual = diff.count();

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
				std::unique_lock<std::mutex> lock4(mutexGameElements);
				gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 0, Move::UP, character->GetPos(), tiempoActual)));
				lock4.unlock();
				cDir.x = 0;
				cDir.y = -1;
			}
			else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)){
				std::unique_lock<std::mutex> lock4(mutexGameElements);
				gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 0, Move::DOWN, character->GetPos(), tiempoActual)));
				lock4.unlock();
				cDir.x = 0;
				cDir.y = 1;
			}
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
				std::unique_lock<std::mutex> lock4(mutexGameElements);
				gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 0, Move::LEFT, character->GetPos(), tiempoActual)));
				lock4.unlock();
				cDir.x = -1;
				cDir.y = 0;
			}
			else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
				std::unique_lock<std::mutex> lock4(mutexGameElements);
				gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 0, Move::RIGHT, character->GetPos(), tiempoActual)));
				lock4.unlock();
				cDir.x = 1;
				cDir.y = 0;
			}
			else {
				std::unique_lock<std::mutex> lock4(mutexGameElements);
				gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 0, Move::REST, character->GetPos(), tiempoActual)));
				lock4.unlock();
			}

			character->Move(cDir);
			// Managing Shooting
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
				cDir.x = 1; // Default shoot direction
				cDir.y = 0; // Default shoot direction

				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
					cDir.y = -1;
					cDir.x = 0;
				}
				else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
					cDir.y = 1;
					cDir.x = 0;
				}
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
					cDir.x = -1;
				else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
					cDir.x = 1;
				Bullet* b = new Bullet(character->GetPos(), cDir, idBullet, sf::Color::Green);
				idBullet += 1;
				std::unique_lock<std::mutex> lock4(mutexGameElements);
				gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 1, b->GetAngle(), b->GetPos(), tiempoActual, b->GetId())));
				lock4.unlock();				
				std::unique_lock<std::mutex> lockBullets(mutexBullets);				
				bullets[b->GetId()] = b;
				lockBullets.unlock();
			}
		}
	}

	
	
	
}

void Game::GetInput()
{
	while (window.pollEvent(event))
	{
		switch (event.type)
		{
		case sf::Event::Closed:
			/**inputtext = "exit";
			*sendText = true;*/
			_keepPlaying->store(false);
			if (onCloseClient) {
				onCloseClient("S");
			}
			//window.close(); // Close windows if X is pressed 
			return;
		case sf::Event::KeyPressed:
			GetInputActions();
			break;
		case sf::Event::MouseButtonPressed:
			if (event.mouseButton.button == sf::Mouse::Left)
			{
				sf::Vector2i mousePos = sf::Mouse::getPosition(window);
				sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

				// Verificar si el mouse hizo clic en el puntero del slider
				if (pointer->getGlobalBounds().contains(mousePosF))
				{
					isDragging = true;
				}
			}
			break;
		case sf::Event::MouseButtonReleased:
			if (event.mouseButton.button == sf::Mouse::Left)
			{
				isDragging = false;
			}
			break;
		case sf::Event::MouseMoved:
			if (isDragging)
			{
				sf::Vector2i mousePos = sf::Mouse::getPosition(window);
				sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));

				// Restringir la posición del puntero dentro de la barra
				if (mousePosF.x < 180)
					mousePosF.x = 180;
				else if (mousePosF.x > 280)
					mousePosF.x = 280;				

				_chancePacketLoss->store(mousePosF.x - 180);
				pointer->setPosition(mousePosF.x, 645);
			}
			break;
		}
	}
}


void Game::DrawTextBox()
{

	//GIU draw when no playing
	window.draw(nameRectangle);
	nameText.setString(*inputtext);
	window.draw(nameText);
	text.setString(*enunciadoText);
	window.draw(text);
	window.draw(textSlider);
	window.draw(*bar);
	window.draw(*pointer);
}

void Game::DrawBackGround()
{
	window.draw(spriteBackground);
}

void Game::DrawEntities()
{	
	// When playing
	window.draw(character->GetSprite());

	if(roomFull && character2 != nullptr)
		window.draw(character2->GetSprite());
	
	//std::chrono::duration<float, std::chrono::milliseconds::period> duration = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::milliseconds::period>>(now.time_since_epoch());
	std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startPlaying);
	float tiempoActual = diff.count();

	// Bullets update
	std::unique_lock<std::mutex> lockBullets(mutexBullets);
	auto it = bullets.begin();
	while (it != bullets.end()) {		
		(*it).second->Move();
		window.draw((*it).second->GetShape());
		std::unique_lock<std::mutex> lock4(mutexGameElements);
		gameElements.insert(std::make_pair(tiempoActual, MoveData(idGame, idPlayer, 1, it->second->GetAngle(), it->second->GetPos(), tiempoActual, it->second->GetId())));
		lock4.unlock();
		it++;
	}
	lockBullets.unlock();

	std::unique_lock<std::mutex> lockBullets2(mutexBullets2);
	auto it2 = bullets2.begin();
	while (it2 != bullets2.end()) {
		(*it2).second->Move();
		window.draw((*it2).second->GetShape());
		it2++;
	}
	lockBullets2.unlock();
}

void Game::Render()
{
	window.clear();

	DrawBackGround();
	DrawTextBox();
	if (*gS == GameState::PLAYING) {
		DrawEntities();
	}
}

void Game::setUp()
{
	// Windows initialization	
	window.create(sf::VideoMode(1250, 700), "Game");

	// Barra del slider
	bar = new sf::RectangleShape(sf::Vector2f(100, 10));
	bar->setPosition(180, 650);
	bar->setFillColor(sf::Color::Blue);

	// Puntero del slider
	pointer = new sf::RectangleShape(sf::Vector2f(10, 30));
	pointer->setOrigin(5, 5);
	pointer->setPosition(180, 645);
	pointer->setFillColor(sf::Color::Red);

	std::filesystem::path directorio_actual = std::filesystem::current_path();
	std::string nombre_ultima_carpeta = directorio_actual.filename().string();

	//std::cout << nombre_ultima_carpeta;

	
	if (nombre_ultima_carpeta == "shooting") {
		ruta_archivo = "resources/";
	}
	else {
		ruta_archivo = "../../shooting/resources/";
	}

	character = new Character(sf::Vector2f(40, 30), (int)!created + 1, ruta_archivo);
	

	if (!font.loadFromFile(ruta_archivo + "fonts/courbd.ttf"))
	{
		std::cout << "Can't load the font file" << std::endl;
	}

	bg.loadFromFile(ruta_archivo + "/bg.png");
	spriteBackground.setTexture(bg);

	//*enunciadoText = "- Type in your name to start playing";

	text = sf::Text(*enunciadoText, font, 14);
	text.setFillColor(sf::Color(0, 0, 0));
	text.setStyle(sf::Text::Italic);
	text.setPosition(860, 50);

	textSlider = sf::Text("Chance packet loss : \n\n                      0%        100% ", font, 14);
	textSlider.setFillColor(sf::Color(255, 255, 255));
	textSlider.setStyle(sf::Text::Italic);
	textSlider.setPosition(0, 645);

	*inputtext = " ";

	nameText = sf::Text(*inputtext, font, 17);
	nameText.setFillColor(sf::Color(0, 0, 255));
	nameText.setStyle(sf::Text::Italic);
	nameText.setPosition(860, 90);//75
	
	nameRectangle = sf::RectangleShape(sf::Vector2f(400, 600));
	nameRectangle.setFillColor(sf::Color(255, 255, 255, 150));
	nameRectangle.setPosition(850, 0);

	//window.draw(sprite);
	//run();
	//while (gS == GameState::CONNECTING) {
	//	window.clear();
	//	window.draw(sprite);
	//	// Manage events when no playing
	//	if ((event.key.code == sf::Keyboard::Delete || event.key.code == sf::Keyboard::BackSpace) && input.getSize() > 0) {
	//		input.erase(input.getSize() - 1, input.getSize());
	//	}
	//	else if (event.key.code == sf::Keyboard::Return && input.getSize() > 0) { gS = GameState::CHALLENGING; break;}
	//	else { input += key2str(event.key.code); }
	//}
	//std::cout << "outGame\n";
	
}

Game::Game(sf::String* _inputtext, sf::String* _enunciadoText, bool* _sendText):inputtext(_inputtext),sendText(_sendText),enunciadoText(_enunciadoText) {
	startPlaying = std::chrono::high_resolution_clock::now();
}

Game::~Game()
{
	if(window.isOpen())
		window.close();

	gS = nullptr;
	if (_chancePacketLoss != nullptr)
		delete _chancePacketLoss;
	_chancePacketLoss = nullptr;
	delete inputtext;
	inputtext = nullptr;
	delete enunciadoText;
	enunciadoText = nullptr;
	delete bar;
	bar = nullptr;
	delete pointer;
	pointer = nullptr;
	delete character;
	character = nullptr;
	delete character2;
	character2 = nullptr;
	tS = nullptr;
}

void Game::run()
{
	*tS = ThreadState::LOOPING;
	setUp(); // Setting Up the GUI
	// App loop
	while (window.isOpen() && _keepPlaying->load())
	{
		GetInput();

		if (!_keepPlaying->load())
			break;

		if (character2 == nullptr && roomFull) {

			character->SetSprite((int)!created + 1, ruta_archivo);
			character2 = new Character(sf::Vector2f(40, 40), (int)created + 1, ruta_archivo);
		}		

		Render();

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		window.display();
	}

	*tS = ThreadState::ENDED;

}
