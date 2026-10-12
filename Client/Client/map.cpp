#include "map.h"

namespace {
	bool IsTown(int x, int y)
	{
		return x >= TOWN_MIN_X && x <= TOWN_MAX_X && y >= TOWN_MIN_Y && y <= TOWN_MAX_Y;
	}

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

void Map::Generate()
{
	tiles.assign(WORLD_WIDTH * WORLD_HEIGHT, TILE_GRASS);

	for (int y = 0; y < WORLD_HEIGHT; ++y) {
		for (int x = 0; x < WORLD_WIDTH; ++x) {
			TileType tile = TILE_GRASS;

			const bool isTown = IsTown(x, y);

			if ((x / 10 + y / 7) % 19 == 0) {
				tile = TILE_SAND;
			}
			if (x > 280 && x < 390 && y > 260 && y < 350) {
				tile = TILE_WATER;
			}
			if ((x * 13 + y * 29) % 211 == 0) {
				tile = TILE_TREE;
			}
			if ((x * 31 + y * 17) % 457 == 0) {
				tile = TILE_ROCK;
			}
			if (isTown) {
				if (IsTownHouse(x, y)) {
					tile = TILE_GRASS;
				}
			}

			tiles[y * WORLD_WIDTH + x] = tile;
		}
	}
}

TileType Map::GetTile(int x, int y) const
{
	if (x < 0 || y < 0 || x >= WORLD_WIDTH || y >= WORLD_HEIGHT || tiles.empty()) {
		return TILE_WATER;
	}

	return tiles[y * WORLD_WIDTH + x];
}

bool Map::IsBlocked(int x, int y) const
{
	const TileType tile = GetTile(x, y);
	return tile == TILE_WATER || tile == TILE_TREE || tile == TILE_ROCK || tile == TILE_HOUSE || IsTownHouse(x, y);
}