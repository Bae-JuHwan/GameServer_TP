#include "server_world.h"

#include "protocol_2026.h"

#include <algorithm>
#include <random>

namespace {
	constexpr int MAP_SECTION_COLS = 3;
	constexpr int MAP_SECTION_ROWS = 3;
	constexpr int TOWN_SECTION_X = 1;
	constexpr int TOWN_SECTION_Y = 1;

	constexpr int MapSectionMinX(int sectionX)
	{
		return WORLD_WIDTH * sectionX / MAP_SECTION_COLS;
	}

	constexpr int MapSectionMaxX(int sectionX)
	{
		return WORLD_WIDTH * (sectionX + 1) / MAP_SECTION_COLS - 1;
	}

	constexpr int MapSectionMinY(int sectionY)
	{
		return WORLD_HEIGHT * sectionY / MAP_SECTION_ROWS;
	}

	constexpr int MapSectionMaxY(int sectionY)
	{
		return WORLD_HEIGHT * (sectionY + 1) / MAP_SECTION_ROWS - 1;
	}

	constexpr int TOWN_MIN_X = MapSectionMinX(TOWN_SECTION_X);
	constexpr int TOWN_MAX_X = MapSectionMaxX(TOWN_SECTION_X);
	constexpr int TOWN_MIN_Y = MapSectionMinY(TOWN_SECTION_Y);
	constexpr int TOWN_MAX_Y = MapSectionMaxY(TOWN_SECTION_Y);

	bool IsRect(int x, int y, int left, int top, int right, int bottom)
	{
		return x >= left && x <= right && y >= top && y <= bottom;
	}

	bool IsTownHouse(int x, int y)
	{
		return IsRect(x, y, 760, 760, 766, 766) ||
			IsRect(x, y, 1140, 760, 1146, 766) ||
			IsRect(x, y, 760, 1100, 766, 1106) ||
			IsRect(x, y, 1140, 1100, 1146, 1106);
	}

}

int ClampInt(int value, int low, int high)
{
	return std::max(low, std::min(value, high));
}

bool IsBlockedTile(int x, int y)
{
	if (x < 0 || y < 0 || x >= WORLD_WIDTH || y >= WORLD_HEIGHT) {
		return true;
	}
	if (IsTownHouse(x, y)) {
		return true;
	}
	if (x > 280 && x < 390 && y > 260 && y < 350) {
		return true;
	}
	if ((x * 13 + y * 29) % 211 == 0) {
		return true;
	}
	if ((x * 31 + y * 17) % 457 == 0) {
		return true;
	}

	return false;
}

void RandomOpenPosition(short& x, short& y)
{
	static std::random_device randomDevice;
	static std::mt19937 generator(randomDevice());
	std::uniform_int_distribution<int> xDistribution(TOWN_MIN_X, TOWN_MAX_X);
	std::uniform_int_distribution<int> yDistribution(TOWN_MIN_Y, TOWN_MAX_Y);

	for (int attempt = 0; attempt < 1000; ++attempt) {
		const int randomX = xDistribution(generator);
		const int randomY = yDistribution(generator);
		if (!IsBlockedTile(randomX, randomY)) {
			x = static_cast<short>(randomX);
			y = static_cast<short>(randomY);
			return;
		}
	}

	x = static_cast<short>((TOWN_MIN_X + TOWN_MAX_X) / 2);
	y = static_cast<short>((TOWN_MIN_Y + TOWN_MAX_Y) / 2);
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

void RandomOpenPositionInSection(int sectionX, int sectionY, short& x, short& y)
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
		const int randomX = xDistribution(generator);
		const int randomY = yDistribution(generator);
		if (!IsBlockedTile(randomX, randomY)) {
			x = static_cast<short>(randomX);
			y = static_cast<short>(randomY);
			return;
		}
	}

	x = static_cast<short>((minX + maxX) / 2);
	y = static_cast<short>((minY + maxY) / 2);
}