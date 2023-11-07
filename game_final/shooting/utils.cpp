#include "utils.h"

MoveData::MoveData(std::string info, float _size, float _speed, int _playerId)
{
    //std::string aux = "T0.3_E0_M2_P0.3-0.5_MV0";    
    //std::string aux = "T0.3_E0_M2_P0.3-0.5_A0.4-0.2_B10";    
    size = _size;
    speed = _speed;
    pId = _playerId;

    std::size_t postime = info.find('T');
    std::size_t posEnd = info.find('_');
    time = std::stof(info.substr(postime + 1, posEnd - postime - 1));
    info.erase(postime, posEnd - postime + 1);

    std::size_t posEntity = info.find('E');
    posEnd = info.find('_');
    isBullet = std::stoi(info.substr(posEntity + 1, posEnd - posEntity - 1));
    info.erase(posEntity, posEnd - posEntity + 1);

    std::size_t posMatch = info.find('M');
    posEnd = info.find('_');
    macthId = std::stoi(info.substr(posMatch + 1, posEnd - posMatch - 1));
    info.erase(posMatch, posEnd - posMatch + 1);

    std::size_t posPosX = info.find('P');
    std::size_t posEndValue = info.find('/');
    posEnd = info.find('_');
    pos.x = std::stof(info.substr(posPosX + 1, posEndValue - posPosX - 1));
    pos.y = std::stof(info.substr(posEndValue + 1, posEnd - posEndValue - 1));
    info.erase(posPosX, posEnd - posPosX + 1);

    if (!isBullet) {
        std::size_t posMatch = info.find("MV"); //TODO
        posEnd = info.find('_');
        move = static_cast<Move>(std::stoi(info.substr(posMatch + 2, posEnd - posMatch - 2)));
    }
    else {
        std::size_t posAngle = info.find('A');
        posEndValue = info.find('/');
        posEnd = info.find('_');
        angle.x = std::stof(info.substr(posAngle + 1, posEndValue - posAngle - 1));
        angle.y = std::stof(info.substr(posEndValue + 1, posEnd - posEndValue - 1));
        info.erase(posAngle, posEnd - posAngle + 1);

        std::size_t posBID = info.find('B');
        posEnd = info.find('_');
        if (posEnd != std::string::npos) {
            bulletId = std::stoi(info.substr(posBID + 1, posEnd - posBID - 1));
            info.erase(posBID, posEnd - posBID + 1);
        }
        else {
            bulletId = std::stoi(info.substr(posBID + 1));
            info.erase(posBID);
        }
    }

}

bool MoveData::MoveCorrectAndResponse()
{
    sf::Vector2f auxPos;

    if (!isBullet) {

        switch (move)
        {
        case Move::REST:
            auxPos.x = 0;
            auxPos.y = 0;
            break;
        case Move::UP:
            auxPos.x = 0;
            auxPos.y = -1;
            break;
        case Move::DOWN:
            auxPos.x = 0;
            auxPos.y = 1;
            break;
        case Move::LEFT:
            auxPos.x = -1;
            auxPos.y = 0;
            break;
        case Move::RIGHT:
            auxPos.x = 1;
            auxPos.y = 0;
            break;
        default:
            break;
        }

        auxPos += pos;
        /*auxPos.x *= size;
        auxPos.y *= size;*/

        bool correct = true;

        if (auxPos.x /** size*/ < 5)
        {
            pos.x = 5;
            correct = false;
        }
        if (auxPos.y /** size*/ < 18)
        {
            pos.y = 18;
            correct = false;

        }
        if (auxPos.x /** size*/ > 80)//820
        {
            pos.x = 80;
            correct = false;

        }
        if (auxPos.y /** size*/ > 52)//560
        {
            pos.y = 52;
            correct = false;

        }

        return correct;

    }
    else {
        //bullet.setPosition(pos * SIZE);
        auxPos = pos + angle * speed;
        if (auxPos.x < 5 || auxPos.y < 18 || auxPos.x > 80 || auxPos.y > 52)
        {
            return false;
        }
        return true;

    }

}

std::string MoveData::SerializeData()
{
    std::string storedData;

    storedData += "T" + std::to_string(time) + "_E" + std::to_string(isBullet) + "_M" + std::to_string(macthId) + "_P" + std::to_string(pos.x) + "/" + std::to_string(pos.y);
    if (isBullet == 0) {
        // Player std::string aux = "T0.3_E0_M2_P0.3-0.5_MV0";    
        storedData += "_MV" + std::to_string(static_cast<int>(move));
    }
    else {
        // Bullet std::string aux = "T0.3_E1_M2_P0.3-0.5_A0.4-0.2_B10";
        storedData += "_A" + std::to_string(angle.x) + "/" + std::to_string(angle.y) + "_B" + std::to_string(bulletId);
    }

    return storedData;
}

std::string key2str(const sf::Keyboard::Key k) {
    std::string ret;
    switch (k) {

    case sf::Keyboard::A:

        ret = "A";
        break;
    case sf::Keyboard::B:

        ret = "B";
        break;
    case sf::Keyboard::C:

        ret = "C";
        break;
    case sf::Keyboard::D:

        ret = "D";
        break;
    case sf::Keyboard::E:

        ret = "E";
        break;
    case sf::Keyboard::F:

        ret = "F";
        break;
    case sf::Keyboard::G:

        ret = "G";
        break;
    case sf::Keyboard::H:

        ret = "H";
        break;
    case sf::Keyboard::I:

        ret = "I";
        break;
    case sf::Keyboard::J:

        ret = "J";
        break;
    case sf::Keyboard::K:

        ret = "K";
        break;
    case sf::Keyboard::L:

        ret = "L";
        break;
    case sf::Keyboard::M:

        ret = "M";
        break;
    case sf::Keyboard::N:

        ret = "N";
        break;
    case sf::Keyboard::O:

        ret = "O";
        break;
    case sf::Keyboard::P:

        ret = "P";
        break;
    case sf::Keyboard::Q:

        ret = "Q";
        break;
    case sf::Keyboard::R:

        ret = "R";
        break;
    case sf::Keyboard::S:

        ret = "S";
        break;
    case sf::Keyboard::T:

        ret = "T";
        break;
    case sf::Keyboard::U:

        ret = "U";
        break;
    case sf::Keyboard::V:

        ret = "V";
        break;
    case sf::Keyboard::W:

        ret = "W";
        break;
    case sf::Keyboard::X:

        ret = "X";
        break;
    case sf::Keyboard::Y:

        ret = "Y";
        break;
    case sf::Keyboard::Z:

        ret = "Z";
        break;
    case sf::Keyboard::Num0:

        ret = "0";
        break;
    case sf::Keyboard::Num1:

        ret = "1";
        break;
    case sf::Keyboard::Num2:

        ret = "2";
        break;
    case sf::Keyboard::Num3:

        ret = "3";
        break;
    case sf::Keyboard::Num4:

        ret = "4";
        break;
    case sf::Keyboard::Num5:

        ret = "5";
        break;
    case sf::Keyboard::Num6:

        ret = "6";
        break;
    case sf::Keyboard::Num7:

        ret = "7";
        break;
    case sf::Keyboard::Num8:

        ret = "8";
        break;
    case sf::Keyboard::Num9:
        ret = "9";
        break;

    case sf::Keyboard::Numpad0:
        ret = "0";
        break;
    case sf::Keyboard::Numpad1:

        ret = "1";
        break;
    case sf::Keyboard::Numpad2:

        ret = "2";
        break;
    case sf::Keyboard::Numpad3:

        ret = "3";
        break;
    case sf::Keyboard::Numpad4:

        ret = "4";
        break;
    case sf::Keyboard::Numpad5:

        ret = "5";
        break;
    case sf::Keyboard::Numpad6:

        ret = "6";
        break;
    case sf::Keyboard::Numpad7:

        ret = "7";
        break;
    case sf::Keyboard::Numpad8:

        ret = "8";
        break;
    case sf::Keyboard::Numpad9:

        ret = "9";
        break;

    default:
        ret = "";
        break;
    }
    return ret;

}


