#include "server_types.h"

std::mutex clientsMutex;
std::vector<std::shared_ptr<ClientState>> clients;
std::vector<MonsterState> monsters;
std::unordered_set<int> monsterTiles;
std::vector<PotionState> potions;
std::unordered_set<int> potionTiles;
std::vector<NpcState> npcs;
std::unordered_set<int> npcTiles;