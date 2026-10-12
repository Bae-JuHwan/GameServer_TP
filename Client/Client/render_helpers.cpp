#include "render_helpers.h"

#include "client_config.h"

sf::IntRect TileRect(TileType tile)
{
	const int tileIndex = static_cast<int>(tile);
	return sf::IntRect(tileIndex * TILE_SOURCE_SIZE, 0, TILE_SOURCE_SIZE, TILE_SOURCE_SIZE);
}

sf::IntRect CharacterRect(int direction, int frame)
{
	return sf::IntRect(
		frame * CharacterFrameSize,
		direction * CharacterFrameSize,
		CharacterFrameSize,
		CharacterFrameSize);
}

sf::IntRect PlayerAttackRect(int direction, int frame)
{
	return sf::IntRect(
		frame * PlayerAttackFrameSize,
		direction * PlayerAttackFrameSize,
		PlayerAttackFrameSize,
		PlayerAttackFrameSize);
}

sf::IntRect PurpleHouseRect()
{
	return sf::IntRect(PurpleHouseSourceX, PurpleHouseSourceY, PurpleHouseSourceWidth, PurpleHouseSourceHeight);
}