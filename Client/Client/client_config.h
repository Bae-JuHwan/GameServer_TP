#pragma once

#include "map.h"

constexpr int CharacterFrameSize = 32;
constexpr int CharacterFrameCount = 4;
constexpr float CharacterScale = 2.0f;
constexpr float MonsterScale = 2.0f;
constexpr int PlayerAttackFrameSize = 64;
constexpr int PlayerAttackFrameCount = 4;
constexpr int PlayerAttackFrameMilliseconds = 70;
constexpr float PlayerAttackScale = 1.2f;

constexpr int PurpleHouseSourceX = 560;
constexpr int PurpleHouseSourceY = 112;
constexpr int PurpleHouseSourceWidth = 112;
constexpr int PurpleHouseSourceHeight = 112;
constexpr int PurpleHouseTileWidth = PurpleHouseSourceWidth / TILE_SOURCE_SIZE;
constexpr int PurpleHouseTileHeight = PurpleHouseSourceHeight / TILE_SOURCE_SIZE;