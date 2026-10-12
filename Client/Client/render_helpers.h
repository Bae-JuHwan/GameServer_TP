#pragma once

#include <SFML/Graphics.hpp>

#include "map.h"

sf::IntRect TileRect(TileType tile);
sf::IntRect CharacterRect(int direction, int frame);
sf::IntRect PlayerAttackRect(int direction, int frame);
sf::IntRect PurpleHouseRect();