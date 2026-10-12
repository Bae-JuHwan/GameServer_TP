#include "npc_spawner.h"

#include "server_types.h"
#include "server_world.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <random>

namespace {
	constexpr int NpcIdStart = NPC_ID_START + NUM_NPCS + 100000;
	constexpr int NpcHomeRadius = 10;

	struct HousePosition {
		short x;
		short y;
	};

	const std::array<HousePosition, 4> TownHouses = {
		HousePosition{760, 760},
		HousePosition{1140, 760},
		HousePosition{760, 1100},
		HousePosition{1140, 1100}
	};

	bool IsOccupied(short x, short y)
	{
		const int tileKey = TileKey(x, y);
		return monsterTiles.find(tileKey) != monsterTiles.end() ||
			potionTiles.find(tileKey) != potionTiles.end() ||
			npcTiles.find(tileKey) != npcTiles.end();
	}

	bool RandomNpcPosition(short homeX, short homeY, short& x, short& y)
	{
		static std::random_device randomDevice;
		static std::mt19937 generator(randomDevice());
		std::uniform_int_distribution<int> offsetDistribution(-NpcHomeRadius, NpcHomeRadius);

		for (int attempt = 0; attempt < 1000; ++attempt) {
			const short candidateX = static_cast<short>(homeX + offsetDistribution(generator));
			const short candidateY = static_cast<short>(homeY + offsetDistribution(generator));
			if (std::abs(static_cast<int>(candidateX) - static_cast<int>(homeX)) > NpcHomeRadius ||
				std::abs(static_cast<int>(candidateY) - static_cast<int>(homeY)) > NpcHomeRadius) {
				continue;
			}
			if (!IsBlockedTile(candidateX, candidateY) && !IsOccupied(candidateX, candidateY)) {
				x = candidateX;
				y = candidateY;
				return true;
			}
		}

		return false;
	}
}

void SpawnTownNpcs()
{
	npcs.clear();
	npcTiles.clear();

	for (const HousePosition& house : TownHouses) {
		NpcState npc{};
		npc.objectId = NpcIdStart + static_cast<int>(npcs.size());
		npc.homeX = static_cast<short>(house.x + 3);
		npc.homeY = static_cast<short>(house.y + 7);

		if (RandomNpcPosition(npc.homeX, npc.homeY, npc.x, npc.y)) {
			npcTiles.insert(TileKey(npc.x, npc.y));
			npcs.push_back(npc);
		}
	}

	std::cout << "Spawned " << npcs.size() << " town NPCs.\n";
}