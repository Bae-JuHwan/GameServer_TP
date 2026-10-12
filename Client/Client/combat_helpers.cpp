#include "combat_helpers.h"

sf::Vector2i DirectionOffset(int direction)
{
	switch (direction) {
	case 1:
		return sf::Vector2i(-1, 0);
	case 2:
		return sf::Vector2i(1, 0);
	case 3:
		return sf::Vector2i(0, -1);
	case 4:
		return sf::Vector2i(-1, -1);
	case 5:
		return sf::Vector2i(1, -1);
	case 6:
		return sf::Vector2i(-1, 1);
	case 7:
		return sf::Vector2i(1, 1);
	default:
		return sf::Vector2i(0, 1);
	}
}

int AttackEffectDirection(int direction)
{
	switch (direction) {
	case 4:
	case 5:
		return 3;
	case 6:
	case 7:
		return 0;
	default:
		return direction;
	}
}

float AttackEffectRotation(int direction)
{
	switch (direction) {
	case 4:
		return -45.0f;
	case 5:
		return 45.0f;
	case 6:
		return 45.0f;
	case 7:
		return -45.0f;
	default:
		return 0.0f;
	}
}