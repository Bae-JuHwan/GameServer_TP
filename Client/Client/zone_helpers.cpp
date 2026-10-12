#include "zone_helpers.h"

#include "map.h"

bool IsTownSectorPosition(int x, int y)
{
	return x >= TOWN_MIN_X && x <= TOWN_MAX_X && y >= TOWN_MIN_Y && y <= TOWN_MAX_Y;
}