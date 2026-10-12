#include "monster_spawner.h"

#include "protocol_2026.h"
#include "server_types.h"
#include "server_world.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <lua.hpp>

namespace {
	struct MonsterTypeStats {
		int hp = MonsterInitialHp;
		int attackDamage = MonsterInitialAttackDamage;
		unsigned long long expReward = MonsterKillExpReward;
	};

	std::unordered_map<int, MonsterTypeStats> monsterTypeStats;

	int SpriteIdFromName(const std::string& spriteName)
	{
		std::istringstream numberStream(spriteName);
		int numericSpriteId = 1;
		numberStream >> numericSpriteId;
		if (!numberStream.fail()) {
			return ClampInt(numericSpriteId, 1, 16);
		}

		const std::string prefix = "menusprite";
		if (spriteName.rfind(prefix, 0) != 0) {
			return 1;
		}

		std::istringstream stream(spriteName.substr(prefix.size()));
		int spriteId = 1;
		stream >> spriteId;
		return ClampInt(spriteId, 1, 16);
	}

	int LuaMonsterType(lua_State* lua)
	{
		const int monsterTypeId = static_cast<int>(luaL_checkinteger(lua, 1));
		MonsterTypeStats stats{};
		if (lua_gettop(lua) >= 5) {
			stats.hp = static_cast<int>(luaL_checkinteger(lua, 5));
			stats.hp = std::max(1, stats.hp);
		}
		if (lua_gettop(lua) >= 6) {
			stats.attackDamage = static_cast<int>(luaL_checkinteger(lua, 6));
			stats.attackDamage = std::max(0, stats.attackDamage);
		}
		if (lua_gettop(lua) >= 7) {
			stats.expReward = static_cast<unsigned long long>(std::max<lua_Integer>(0, luaL_checkinteger(lua, 7)));
		}

		monsterTypeStats[monsterTypeId] = stats;
		return 0;
	}

	int LuaSection(lua_State* lua)
	{
		const int mapNumber = static_cast<int>(luaL_checkinteger(lua, 1));
		const int monsterTypeId = static_cast<int>(luaL_checkinteger(lua, 2));
		const int count = static_cast<int>(luaL_checkinteger(lua, 3));

		int sectionX = 0;
		int sectionY = 0;
		if (!SectionFromMapNumber(mapNumber, sectionX, sectionY)) {
			return 0;
		}

		const int spriteId = SpriteIdFromName(std::to_string(monsterTypeId));
		const auto stats = monsterTypeStats.find(monsterTypeId);
		for (int i = 0; i < count; ++i) {
			MonsterState monster{};
			monster.objectId = NPC_ID_START + static_cast<int>(monsters.size());
			monster.spriteId = spriteId;
			if (stats != monsterTypeStats.end()) {
				monster.hp = stats->second.hp;
				monster.maxHp = stats->second.hp;
				monster.attackDamage = stats->second.attackDamage;
				monster.expReward = stats->second.expReward;
			}

			for (int attempt = 0; attempt < 1000; ++attempt) {
				RandomOpenPositionInSection(sectionX, sectionY, monster.x, monster.y);
				if (monsterTiles.insert(TileKey(monster.x, monster.y)).second) {
					monsters.push_back(monster);
					break;
				}
			}
		}

		return 0;
	}
}

void LoadMonsterSpawnScript(const std::array<std::string, 4>& paths)
{
	for (const std::string& path : paths) {
		std::ifstream file(path);
		if (!file.is_open()) {
			continue;
		}

		file.close();

		lua_State* lua = luaL_newstate();
		if (!lua) {
			std::cout << "Could not create Lua state.\n";
			return;
		}

		luaL_openlibs(lua);
		monsterTypeStats.clear();
		lua_register(lua, "monster_type", LuaMonsterType);
		lua_register(lua, "section", LuaSection);

		if (luaL_dofile(lua, path.c_str()) != LUA_OK) {
			std::cout << "Lua error: " << lua_tostring(lua, -1) << '\n';
			lua_close(lua);
			return;
		}

		lua_close(lua);
		return;
	}

	std::cout << "Could not load monster spawn script.\n";
}