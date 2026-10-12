#pragma once

#include <array>
#include <string>
#include <vector>

#include "client_types.h"
#include "map.h"

struct MonsterTypeDefinition {
	int spriteId = 1;
	std::string name;
	std::string rightTexturePath;
	std::string leftTexturePath;
};

int SpriteIdFromName(const std::string& spriteName);
void LoadMonsterTypes(std::vector<MonsterTypeDefinition>& monsterTypes, const std::array<std::string, 4>& paths);
void LoadMonsterSpawnScript(const Map& map, std::vector<Monster>& monsters, const std::array<std::string, 4>& paths);