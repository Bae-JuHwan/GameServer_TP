#include "monster_spawner.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <lua.hpp>

#include "world_utils.h"

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

namespace {
	struct MonsterTypeContext {
		std::vector<MonsterTypeDefinition>* monsterTypes = nullptr;
	};

	struct MonsterSpawnContext {
		const Map* map = nullptr;
		std::vector<Monster>* monsters = nullptr;
	};

	int LuaMonsterType(lua_State* lua)
	{
		MonsterTypeContext* context = static_cast<MonsterTypeContext*>(lua_touserdata(lua, lua_upvalueindex(1)));
		if (!context || !context->monsterTypes) {
			return 0;
		}

		MonsterTypeDefinition monsterType{};
		monsterType.spriteId = SpriteIdFromName(luaL_checkstring(lua, 1));
		monsterType.name = luaL_checkstring(lua, 2);
		monsterType.rightTexturePath = luaL_checkstring(lua, 3);
		monsterType.leftTexturePath = luaL_checkstring(lua, 4);
		if (!monsterType.rightTexturePath.empty() && !monsterType.leftTexturePath.empty()) {
			context->monsterTypes->push_back(monsterType);
		}

		return 0;
	}

	int LuaSection(lua_State* lua)
	{
		MonsterSpawnContext* context = static_cast<MonsterSpawnContext*>(lua_touserdata(lua, lua_upvalueindex(1)));
		if (!context || !context->map || !context->monsters) {
			return 0;
		}

		const int mapNumber = static_cast<int>(luaL_checkinteger(lua, 1));
		const std::string spriteName = luaL_checkstring(lua, 2);
		const int count = static_cast<int>(luaL_checkinteger(lua, 3));

		int sectionX = 0;
		int sectionY = 0;
		if (!SectionFromMapNumber(mapNumber, sectionX, sectionY)) {
			return 0;
		}

		const int spriteId = SpriteIdFromName(spriteName);
		for (int i = 0; i < count; ++i) {
			const sf::Vector2i position = RandomOpenPositionInSection(*context->map, sectionX, sectionY);
			context->monsters->push_back(Monster{ -1, position.x, position.y, spriteId });
		}

		return 0;
	}

	int LuaNoOp(lua_State* lua)
	{
		return 0;
	}

	void RegisterLuaFunction(lua_State* lua, const char* name, lua_CFunction function, void* context)
	{
		lua_pushlightuserdata(lua, context);
		lua_pushcclosure(lua, function, 1);
		lua_setglobal(lua, name);
	}
}

void LoadMonsterTypes(std::vector<MonsterTypeDefinition>& monsterTypes, const std::array<std::string, 4>& paths)
{
	monsterTypes.clear();

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
		MonsterTypeContext context{ &monsterTypes };
		RegisterLuaFunction(lua, "monster_type", LuaMonsterType, &context);
		lua_register(lua, "section", LuaNoOp);

		if (luaL_dofile(lua, path.c_str()) != LUA_OK) {
			std::cout << "Lua error: " << lua_tostring(lua, -1) << '\n';
			lua_close(lua);
			return;
		}

		lua_close(lua);
		return;
	}
}

void LoadMonsterSpawnScript(const Map& map, std::vector<Monster>& monsters, const std::array<std::string, 4>& paths)
{
	for (const std::string& path : paths) {
		std::ifstream file(path);
		if (!file.is_open()) {
			continue;
		}

		file.close();

		const std::size_t monsterCountBeforeLoad = monsters.size();
		lua_State* lua = luaL_newstate();
		if (!lua) {
			std::cout << "Could not create Lua state.\n";
			return;
		}

		luaL_openlibs(lua);
		MonsterSpawnContext context{ &map, &monsters };
		lua_register(lua, "monster_type", LuaNoOp);
		RegisterLuaFunction(lua, "section", LuaSection, &context);

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