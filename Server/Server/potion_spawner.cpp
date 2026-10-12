#include "potion_spawner.h"

#include "server_types.h"
#include "server_world.h"

#include <iostream>

namespace {
	constexpr int PotionIdStart = NPC_ID_START + NUM_NPCS;
	constexpr int PotionHealAmount = 20;
	constexpr int PotionCountPerSection = 1000;

	bool IsOccupied(short x, short y)
	{
		const int tileKey = TileKey(x, y);
		return monsterTiles.find(tileKey) != monsterTiles.end() ||
			potionTiles.find(tileKey) != potionTiles.end();
	}
}

void SpawnPotions()
{
	potions.clear();
	potionTiles.clear();

	for (int mapNumber = 1; mapNumber <= 8; ++mapNumber) {
		int sectionX = 0;
		int sectionY = 0;
		if (!SectionFromMapNumber(mapNumber, sectionX, sectionY)) {
			continue;
		}

		for (int potionIndex = 0; potionIndex < PotionCountPerSection; ++potionIndex) {
			PotionState potion{};
			potion.objectId = PotionIdStart + static_cast<int>(potions.size());
			potion.visualId = 1;
			potion.healAmount = PotionHealAmount;

			for (int attempt = 0; attempt < 1000; ++attempt) {
				RandomOpenPositionInSection(sectionX, sectionY, potion.x, potion.y);
				if (!IsOccupied(potion.x, potion.y)) {
					potionTiles.insert(TileKey(potion.x, potion.y));
					potions.push_back(potion);
					break;
				}
			}
		}
	}

	std::cout << "Spawned " << potions.size() << " potions.\n";
}