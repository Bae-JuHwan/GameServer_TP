#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>

#include "map.h"

int ClampInt(int value, int low, int high);
sf::Vector2i RandomOpenPosition(const Map& map);
bool SectionFromNumberKey(sf::Keyboard::Key key, int& sectionX, int& sectionY);
bool SectionFromMapNumber(int number, int& sectionX, int& sectionY);
sf::Vector2i RandomOpenPositionInSection(const Map& map, int sectionX, int sectionY);