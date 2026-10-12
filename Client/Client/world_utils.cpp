#include "world_utils.h"

#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <random>

int ClampInt(int value, int low, int high)
{
	return std::max(low, std::min(value, high));
}

sf::Vector2i RandomOpenPosition(const Map& map)
{
	static std::random_device randomDevice;
	static std::mt19937 generator(randomDevice());
	std::uniform_int_distribution<int> xDistribution(TOWN_MIN_X, TOWN_MAX_X);
	std::uniform_int_distribution<int> yDistribution(TOWN_MIN_Y, TOWN_MAX_Y);

	for (int attempt = 0; attempt < 1000; ++attempt) {
		const int x = xDistribution(generator);
		const int y = yDistribution(generator);
		if (!map.IsBlocked(x, y)) {
			return sf::Vector2i(x, y);
		}
	}

	return sf::Vector2i((TOWN_MIN_X + TOWN_MAX_X) / 2, (TOWN_MIN_Y + TOWN_MAX_Y) / 2);
}

bool SectionFromNumberKey(sf::Keyboard::Key key, int& sectionX, int& sectionY)
{
	switch (key) {
	case sf::Keyboard::Num0:
	case sf::Keyboard::Numpad0:
		sectionX = 1;
		sectionY = 1;
		return true;
	case sf::Keyboard::Num1:
	case sf::Keyboard::Numpad1:
		sectionX = 1;
		sectionY = 0;
		return true;
	case sf::Keyboard::Num2:
	case sf::Keyboard::Numpad2:
		sectionX = 0;
		sectionY = 1;
		return true;
	case sf::Keyboard::Num3:
	case sf::Keyboard::Numpad3:
		sectionX = 2;
		sectionY = 1;
		return true;
	case sf::Keyboard::Num4:
	case sf::Keyboard::Numpad4:
		sectionX = 1;
		sectionY = 2;
		return true;
	case sf::Keyboard::Num5:
	case sf::Keyboard::Numpad5:
		sectionX = 0;
		sectionY = 0;
		return true;
	case sf::Keyboard::Num6:
	case sf::Keyboard::Numpad6:
		sectionX = 2;
		sectionY = 0;
		return true;
	case sf::Keyboard::Num7:
	case sf::Keyboard::Numpad7:
		sectionX = 0;
		sectionY = 2;
		return true;
	case sf::Keyboard::Num8:
	case sf::Keyboard::Numpad8:
		sectionX = 2;
		sectionY = 2;
		return true;
	default:
		return false;
	}
}

bool SectionFromMapNumber(int number, int& sectionX, int& sectionY)
{
	switch (number) {
	case 0:
		sectionX = 1;
		sectionY = 1;
		return true;
	case 1:
		sectionX = 1;
		sectionY = 0;
		return true;
	case 2:
		sectionX = 0;
		sectionY = 1;
		return true;
	case 3:
		sectionX = 2;
		sectionY = 1;
		return true;
	case 4:
		sectionX = 1;
		sectionY = 2;
		return true;
	case 5:
		sectionX = 0;
		sectionY = 0;
		return true;
	case 6:
		sectionX = 2;
		sectionY = 0;
		return true;
	case 7:
		sectionX = 0;
		sectionY = 2;
		return true;
	case 8:
		sectionX = 2;
		sectionY = 2;
		return true;
	default:
		return false;
	}
}

sf::Vector2i RandomOpenPositionInSection(const Map& map, int sectionX, int sectionY)
{
	static std::random_device randomDevice;
	static std::mt19937 generator(randomDevice());
	const int minX = MapSectionMinX(sectionX);
	const int maxX = MapSectionMaxX(sectionX);
	const int minY = MapSectionMinY(sectionY);
	const int maxY = MapSectionMaxY(sectionY);
	std::uniform_int_distribution<int> xDistribution(minX, maxX);
	std::uniform_int_distribution<int> yDistribution(minY, maxY);

	for (int attempt = 0; attempt < 1000; ++attempt) {
		const int x = xDistribution(generator);
		const int y = yDistribution(generator);
		if (!map.IsBlocked(x, y)) {
			return sf::Vector2i(x, y);
		}
	}

	return sf::Vector2i((minX + maxX) / 2, (minY + maxY) / 2);
}