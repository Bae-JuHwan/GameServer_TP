#pragma once
#include <vector>
#include "protocol_2026.h"

constexpr int SCREEN_TILE_X = 24;
constexpr int SCREEN_TILE_Y = 24;

constexpr int TILE_SOURCE_SIZE = 16;
constexpr int TILE_SIZE = 40;
constexpr int SCREEN_WIDTH = SCREEN_TILE_X * TILE_SIZE;
constexpr int SCREEN_HEIGHT = SCREEN_TILE_Y * TILE_SIZE;

constexpr int MAP_SECTION_COLS = 3;
constexpr int MAP_SECTION_ROWS = 3;

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

constexpr int TOWN_SECTION_X = 1;
constexpr int TOWN_SECTION_Y = 1;
constexpr int TOWN_MIN_X = MapSectionMinX(TOWN_SECTION_X);
constexpr int TOWN_MAX_X = MapSectionMaxX(TOWN_SECTION_X);
constexpr int TOWN_MIN_Y = MapSectionMinY(TOWN_SECTION_Y);
constexpr int TOWN_MAX_Y = MapSectionMaxY(TOWN_SECTION_Y);

enum TileType {
	TILE_GRASS = 0,
	TILE_SAND,
	TILE_WATER,
	TILE_SNOW,
	TILE_TREE,
	TILE_ROCK,
	TILE_HOUSE
};

class Map {
public:
	void Generate();

	TileType GetTile(int x, int y) const;
	bool IsBlocked(int x, int y) const;

private:
	std::vector<TileType> tiles;
};