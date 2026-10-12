#pragma once

#include "server_platform.h"

#include <memory>
#include <mutex>
#include <string>
#include <chrono>
#include <unordered_set>
#include <vector>

#include "protocol_2026.h"

constexpr int PlayerInitialHp = 100;
constexpr int PlayerInitialAttackDamage = 10;
constexpr unsigned char PlayerInitialLevel = 1;
constexpr unsigned char PlayerPowerUpLevel = 10;
constexpr int PlayerPowerUpHp = 200;
constexpr int PlayerPowerUpAttackDamage = 20;
constexpr unsigned char PlayerSecondPowerUpLevel = 20;
constexpr int PlayerSecondPowerUpHp = 400;
constexpr int PlayerSecondPowerUpAttackDamage = 40;
constexpr int MonsterInitialHp = 10;
constexpr int MonsterInitialAttackDamage = 10;
constexpr unsigned long long MonsterKillExpReward = 100;

inline unsigned long long RequiredExpForNextLevel(unsigned char level)
{
	return static_cast<unsigned long long>(level) * 100;
}

struct ClientState {
	SOCKET socket = INVALID_SOCKET;
	int playerId = -1;
	std::string name;
	short x = 0;
	short y = 0;
	int hp = PlayerInitialHp;
	int maxHp = PlayerInitialHp;
	int attackDamage = PlayerInitialAttackDamage;
	unsigned long long exp = 0;
	unsigned char level = PlayerInitialLevel;
	int beginnerQuestKills = 0;
	bool beginnerQuestAccepted = false;
	bool beginnerQuestComplete = false;
	bool hasBeginnerSkill = false;
	int intermediateQuestKills = 0;
	bool intermediateQuestAccepted = false;
	bool intermediateQuestComplete = false;
	bool hasDash = false;
	bool loggedIn = false;
	std::unordered_set<int> visibleMonsters;
	std::unordered_set<int> visiblePotions;
	std::unordered_set<int> visibleNpcs;
};

struct MonsterState {
	int objectId = -1;
	short x = 0;
	short y = 0;
	int spriteId = 1;
	int hp = MonsterInitialHp;
	int maxHp = MonsterInitialHp;
	int attackDamage = MonsterInitialAttackDamage;
	unsigned long long expReward = MonsterKillExpReward;
	std::chrono::steady_clock::time_point lastSkillTime = std::chrono::steady_clock::now();
	int skillCooldownMilliseconds = 2000;
};

struct PotionState {
	int objectId = -1;
	short x = 0;
	short y = 0;
	int visualId = 1;
	int healAmount = 20;
	bool active = true;
};

struct NpcState {
	int objectId = -1;
	short x = 0;
	short y = 0;
	short homeX = 0;
	short homeY = 0;
	int direction = 0;
};

extern std::mutex clientsMutex;
extern std::vector<std::shared_ptr<ClientState>> clients;
extern std::vector<MonsterState> monsters;
extern std::unordered_set<int> monsterTiles;
extern std::vector<PotionState> potions;
extern std::unordered_set<int> potionTiles;
extern std::vector<NpcState> npcs;
extern std::unordered_set<int> npcTiles;

inline int TileKey(short x, short y)
{
	return static_cast<int>(y) * WORLD_WIDTH + static_cast<int>(x);
}