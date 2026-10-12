#include "monster_ai.h"

#include "database.h"
#include "packet_utils.h"
#include "protocol_2026.h"
#include "server_types.h"
#include "server_world.h"

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <random>
#include <thread>
#include <unordered_set>

namespace {
	constexpr int MonsterViewSize = 20;
	constexpr int MonsterViewRadius = MonsterViewSize / 2;
	constexpr int MonsterChaseRadius = 5;
	constexpr int BossChaseRadius = 10;
	constexpr int MonsterMoveMilliseconds = 1000;
	constexpr int BossSpriteId = 16;
	constexpr int SecondBossSpriteId = 3;
	constexpr int ThirdBossSpriteId = 14;
	constexpr int FourthBossSpriteId = 9;
	constexpr int BossSkillVisualIdValue = 16;
	constexpr int SecondBossSkillVisualId = 6;
	constexpr int ThirdBossSkillVisualId = 14;
	constexpr int FourthBossSkillVisualId = 9;
	constexpr int BossSkillRange = 8;
	constexpr int BossSkillStepMilliseconds = 45;
	constexpr int BossSkillCooldownMinMilliseconds = 2000;
	constexpr int BossSkillCooldownMaxMilliseconds = 3000;
	constexpr int BossCollisionHalfWidthTiles = 4;
	constexpr int BossCollisionTopTiles = 7;
	constexpr int NpcIdStart = NPC_ID_START + NUM_NPCS + 100000;
	constexpr int NpcHomeRadius = 20;

	bool IsBossSprite(int spriteId)
	{
		return spriteId == BossSpriteId || spriteId == SecondBossSpriteId || spriteId == ThirdBossSpriteId || spriteId == FourthBossSpriteId;
	}

	int BossSkillVisualId(const MonsterState& monster)
	{
		if (monster.spriteId == SecondBossSpriteId) {
			return SecondBossSkillVisualId;
		}
		if (monster.spriteId == ThirdBossSpriteId) {
			return ThirdBossSkillVisualId;
		}
		if (monster.spriteId == FourthBossSpriteId) {
			return FourthBossSkillVisualId;
		}
		return BossSkillVisualIdValue;
	}

	bool IsBossSkillReady(const MonsterState& monster, std::chrono::steady_clock::time_point now)
	{
		const auto elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
			now - monster.lastSkillTime).count();
		return elapsedMilliseconds >= monster.skillCooldownMilliseconds;
	}

	void ResetBossSkillCooldown(MonsterState& monster, std::mt19937& generator, std::chrono::steady_clock::time_point now)
	{
		std::uniform_int_distribution<int> cooldownDistribution(
			BossSkillCooldownMinMilliseconds,
			BossSkillCooldownMaxMilliseconds);
		monster.lastSkillTime = now;
		monster.skillCooldownMilliseconds = cooldownDistribution(generator);
	}

	bool IsInMonsterView(const ClientState& client, const MonsterState& monster)
	{
		const int viewRadius = MonsterViewRadius + (IsBossSprite(monster.spriteId) ? BossCollisionTopTiles : 0);
		return std::abs(static_cast<int>(monster.x) - static_cast<int>(client.x)) <= viewRadius &&
			std::abs(static_cast<int>(monster.y) - static_cast<int>(client.y)) <= viewRadius;
	}

	bool IsInNpcView(const ClientState& client, const NpcState& npc)
	{
		return std::abs(static_cast<int>(npc.x) - static_cast<int>(client.x)) <= MonsterViewRadius &&
			std::abs(static_cast<int>(npc.y) - static_cast<int>(client.y)) <= MonsterViewRadius;
	}

	bool IsPlayerAt(short x, short y)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && client->x == x && client->y == y) {
				return true;
			}
		}

		return false;
	}

	bool IsInBossCollisionAt(const MonsterState& monster, short bossX, short bossY, short x, short y)
	{
		return IsBossSprite(monster.spriteId) &&
			std::abs(static_cast<int>(bossX) - static_cast<int>(x)) <= BossCollisionHalfWidthTiles &&
			static_cast<int>(y) >= static_cast<int>(bossY) - BossCollisionTopTiles &&
			static_cast<int>(y) <= static_cast<int>(bossY);
	}

	bool WouldBossOverlapPlayer(const MonsterState& monster, short nextX, short nextY)
	{
		if (!IsBossSprite(monster.spriteId)) {
			return IsPlayerAt(nextX, nextY);
		}

		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && client->hp > 0 && IsInBossCollisionAt(monster, nextX, nextY, client->x, client->y)) {
				return true;
			}
		}

		return false;
	}

	int DistanceToBossBody(const MonsterState& monster, const ClientState& client)
	{
		if (!IsBossSprite(monster.spriteId)) {
			return std::abs(static_cast<int>(client.x) - static_cast<int>(monster.x)) +
				std::abs(static_cast<int>(client.y) - static_cast<int>(monster.y));
		}

		const int left = static_cast<int>(monster.x) - BossCollisionHalfWidthTiles;
		const int right = static_cast<int>(monster.x) + BossCollisionHalfWidthTiles;
		const int top = static_cast<int>(monster.y) - BossCollisionTopTiles;
		const int bottom = static_cast<int>(monster.y);
		const int clientX = static_cast<int>(client.x);
		const int clientY = static_cast<int>(client.y);
		const int dx = clientX < left ? left - clientX : (clientX > right ? clientX - right : 0);
		const int dy = clientY < top ? top - clientY : (clientY > bottom ? clientY - bottom : 0);
		return dx + dy;
	}

	bool DirectionFromBossBodyToPlayer(const MonsterState& monster, const ClientState& client, int& direction)
	{
		const int left = static_cast<int>(monster.x) - BossCollisionHalfWidthTiles;
		const int right = static_cast<int>(monster.x) + BossCollisionHalfWidthTiles;
		const int top = static_cast<int>(monster.y) - BossCollisionTopTiles;
		const int bottom = static_cast<int>(monster.y);
		const int clientX = static_cast<int>(client.x);
		const int clientY = static_cast<int>(client.y);

		if (clientX < left) {
			direction = 1;
			return true;
		}
		if (clientX > right) {
			direction = 2;
			return true;
		}
		if (clientY < top) {
			direction = 3;
			return true;
		}
		if (clientY > bottom) {
			direction = 0;
			return true;
		}

		return false;
	}

	bool BossSkillDirectionToPlayer(const MonsterState& monster, const ClientState& client, int& direction)
	{
		const int left = static_cast<int>(monster.x) - BossCollisionHalfWidthTiles;
		const int right = static_cast<int>(monster.x) + BossCollisionHalfWidthTiles;
		const int top = static_cast<int>(monster.y) - BossCollisionTopTiles;
		const int bottom = static_cast<int>(monster.y);
		const int clientX = static_cast<int>(client.x);
		const int clientY = static_cast<int>(client.y);

		if (clientY >= top && clientY <= bottom) {
			if (clientX < left) {
				direction = 1;
				return true;
			}
			if (clientX > right) {
				direction = 2;
				return true;
			}
		}

		if (clientX >= left && clientX <= right) {
			if (clientY < top) {
				direction = 3;
				return true;
			}
			if (clientY > bottom) {
				direction = 0;
				return true;
			}
		}

		return false;
	}

	bool DirectionOffset(int direction, short& dx, short& dy)
	{
		dx = 0;
		dy = 0;
		switch (direction) {
		case 1:
			dx = -1;
			return true;
		case 2:
			dx = 1;
			return true;
		case 3:
			dy = -1;
			return true;
		default:
			dy = 1;
			return true;
		}
	}

	void SendAddMonster(SOCKET clientSocket, const MonsterState& monster)
	{
		S2C_AddObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_ADD_OBJECT;
		packet.object_id = monster.objectId;
		packet.visual_id = monster.spriteId;
		CopyText(packet.obj_name, sizeof(packet.obj_name), "Monster");
		packet.x = monster.x;
		packet.y = monster.y;
		packet.hp = monster.hp;
		packet.max_hp = monster.maxHp;
		packet.exp = 0;
		packet.level = 1;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendAddNpc(SOCKET clientSocket, const NpcState& npc)
	{
		S2C_AddObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_ADD_OBJECT;
		packet.object_id = npc.objectId;
		packet.visual_id = 6;
		CopyText(packet.obj_name, sizeof(packet.obj_name), "NPC");
		packet.x = npc.x;
		packet.y = npc.y;
		packet.hp = 0;
		packet.max_hp = 0;
		packet.exp = 0;
		packet.level = 1;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendRemoveObject(SOCKET clientSocket, int objectId)
	{
		S2C_RemoveObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_REMOVE_OBJECT;
		packet.object_id = objectId;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendMoveObject(SOCKET clientSocket, int objectId, short x, short y, int moveTime)
	{
		S2C_MoveObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_MOVE_OBJECT;
		packet.object_id = objectId;
		packet.x = x;
		packet.y = y;
		packet.move_time = moveTime;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendAttackObject(SOCKET clientSocket, int objectId, int direction)
	{
		S2C_AttackObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_ATTACK_OBJECT;
		packet.object_id = objectId;
		packet.direction = direction;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendBossSkillObject(SOCKET clientSocket, int objectId, int visualId, short x, short y, int direction)
	{
		S2C_SkillObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_BOSS_SKILL_OBJECT;
		packet.object_id = objectId;
		packet.visual_id = visualId;
		packet.x = x;
		packet.y = y;
		packet.direction = direction;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendStatusChange(SOCKET clientSocket, const ClientState& player)
	{
		S2C_StatusChange packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_STATUS_CHANGE;
		packet.object_id = player.playerId;
		packet.hp = player.hp;
		packet.max_hp = player.maxHp;
		packet.exp = player.exp;
		packet.level = player.level;
		packet.has_dash = player.hasDash;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void BroadcastMonsterAttack(const MonsterState& monster, int direction)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && IsInMonsterView(*client, monster)) {
				SendAttackObject(client->socket, monster.objectId, direction);
			}
		}
	}

	void BroadcastBossSkillAttack(const MonsterState& monster, short skillX, short skillY, int direction)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && IsInMonsterView(*client, monster)) {
				SendBossSkillObject(client->socket, monster.objectId, BossSkillVisualId(monster), skillX, skillY, direction);
			}
		}
	}

	void BroadcastPlayerStatus(const ClientState& player)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn) {
				SendStatusChange(client->socket, player);
			}
		}
	}

	void BroadcastPlayerMove(const ClientState& player)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn) {
				SendMoveObject(client->socket, player.playerId, player.x, player.y, 0);
			}
		}
	}

	void RespawnPlayer(ClientState& player)
	{
		RandomOpenPosition(player.x, player.y);
		for (int attempt = 0; attempt < 1000 &&
			(monsterTiles.find(TileKey(player.x, player.y)) != monsterTiles.end() ||
				npcTiles.find(TileKey(player.x, player.y)) != npcTiles.end()); ++attempt) {
			RandomOpenPosition(player.x, player.y);
		}

		player.exp = 0;
		player.level = PlayerInitialLevel;
		player.maxHp = PlayerInitialHp;
		player.hp = player.maxHp;
		player.attackDamage = PlayerInitialAttackDamage;
		player.beginnerQuestAccepted = false;
		player.beginnerQuestKills = 0;
		player.beginnerQuestComplete = false;
		player.hasBeginnerSkill = false;
		player.intermediateQuestAccepted = false;
		player.intermediateQuestKills = 0;
		player.intermediateQuestComplete = false;
		player.hasDash = false;
		for (int objectId : player.visibleMonsters) {
			SendRemoveObject(player.socket, objectId);
		}
		player.visibleMonsters.clear();

		if (g_db.is_connected()) {
			g_db.save_player(player);
		}
		BroadcastPlayerMove(player);
		BroadcastPlayerStatus(player);
	}

	ClientState* FindAdjacentPlayer(const MonsterState& monster, int& direction)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn || client->hp <= 0) {
				continue;
			}

			if (IsBossSprite(monster.spriteId)) {
				if (DistanceToBossBody(monster, *client) == 1 &&
					DirectionFromBossBodyToPlayer(monster, *client, direction)) {
					return client.get();
				}
				continue;
			}

			const int dx = static_cast<int>(client->x) - static_cast<int>(monster.x);
			const int dy = static_cast<int>(client->y) - static_cast<int>(monster.y);
			if (dx == -1 && dy == 0) {
				direction = 1;
				return client.get();
			}
			if (dx == 1 && dy == 0) {
				direction = 2;
				return client.get();
			}
			if (dx == 0 && dy == -1) {
				direction = 3;
				return client.get();
			}
			if (dx == 0 && dy == 1) {
				direction = 0;
				return client.get();
			}
		}

		return nullptr;
	}

	ClientState* FindChaseTarget(const MonsterState& monster)
	{
		ClientState* bestTarget = nullptr;
		const bool isBoss = IsBossSprite(monster.spriteId);
		const int chaseRadius = isBoss ? BossChaseRadius : MonsterChaseRadius;
		int bestDistance = chaseRadius * 2 + 1;

		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn || client->hp <= 0) {
				continue;
			}

			if (isBoss) {
				const int distance = DistanceToBossBody(monster, *client);
				if (distance > chaseRadius || distance >= bestDistance) {
					continue;
				}

				bestDistance = distance;
				bestTarget = client.get();
				continue;
			}

			const int dx = static_cast<int>(client->x) - static_cast<int>(monster.x);
			const int dy = static_cast<int>(client->y) - static_cast<int>(monster.y);
			if (std::abs(dx) > chaseRadius || std::abs(dy) > chaseRadius) {
				continue;
			}

			const int distance = std::abs(dx) + std::abs(dy);
			if (distance < bestDistance) {
				bestDistance = distance;
				bestTarget = client.get();
			}
		}

		return bestTarget;
	}

	bool IsBoss(const MonsterState& monster)
	{
		return IsBossSprite(monster.spriteId);
	}

	ClientState* FindBossSkillTarget(const MonsterState& monster, int& direction)
	{
		ClientState* bestTarget = nullptr;
		int bestDistance = BossSkillRange + 1;

		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn || client->hp <= 0) {
				continue;
			}

			int candidateDirection = 0;
			if (!BossSkillDirectionToPlayer(monster, *client, candidateDirection)) {
				continue;
			}

			const int distance = DistanceToBossBody(monster, *client);
			if (distance == 0 || distance > BossSkillRange || distance >= bestDistance) {
				continue;
			}

			bestDistance = distance;
			bestTarget = client.get();
			direction = candidateDirection;
		}

		return bestTarget;
	}

	void BossSkillStartPosition(const MonsterState& monster, const ClientState& target, int direction, short& skillX, short& skillY)
	{
		switch (direction) {
		case 1:
			skillX = static_cast<short>(monster.x - BossCollisionHalfWidthTiles);
			skillY = target.y;
			break;
		case 2:
			skillX = static_cast<short>(monster.x + BossCollisionHalfWidthTiles);
			skillY = target.y;
			break;
		case 3:
			skillX = target.x;
			skillY = static_cast<short>(monster.y - BossCollisionTopTiles);
			break;
		default:
			skillX = target.x;
			skillY = monster.y;
			break;
		}
	}

	void DamagePlayer(ClientState& target, int damage)
	{
		target.hp = std::max(0, target.hp - damage);
		if (target.hp <= 0) {
			RespawnPlayer(target);
		}
		else {
			if (g_db.is_connected()) {
				g_db.save_player(target);
			}
			BroadcastPlayerStatus(target);
		}
	}

	void ApplyBossSkillDamageStep(short startX, short startY, int direction, int distance, int damage)
	{
		short dx = 0;
		short dy = 0;
		DirectionOffset(direction, dx, dy);

		const short skillX = static_cast<short>(startX + dx * distance);
		const short skillY = static_cast<short>(startY + dy * distance);

		std::lock_guard<std::mutex> lock(clientsMutex);
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn || client->hp <= 0 || client->x != skillX || client->y != skillY) {
				continue;
			}

			DamagePlayer(*client, damage);
			return;
		}
	}

	void RunBossSkillDamage(short startX, short startY, int direction, int damage)
	{
		for (int distance = 1; distance <= BossSkillRange; ++distance) {
			std::this_thread::sleep_for(std::chrono::milliseconds(BossSkillStepMilliseconds));
			ApplyBossSkillDamageStep(startX, startY, direction, distance, damage);
		}
	}

	bool TryMoveMonster(MonsterState& monster, short nextX, short nextY)
	{
		if (IsBlockedTile(nextX, nextY) || WouldBossOverlapPlayer(monster, nextX, nextY)) {
			return false;
		}

		const int nextTileKey = TileKey(nextX, nextY);
		if (monsterTiles.find(nextTileKey) != monsterTiles.end()) {
			return false;
		}

		monsterTiles.erase(TileKey(monster.x, monster.y));
		monsterTiles.insert(nextTileKey);
		monster.x = nextX;
		monster.y = nextY;
		return true;
	}

	bool TryMoveNpc(NpcState& npc, short nextX, short nextY)
	{
		if (std::abs(static_cast<int>(nextX) - static_cast<int>(npc.homeX)) > NpcHomeRadius ||
			std::abs(static_cast<int>(nextY) - static_cast<int>(npc.homeY)) > NpcHomeRadius) {
			return false;
		}
		if (IsBlockedTile(nextX, nextY) || IsPlayerAt(nextX, nextY)) {
			return false;
		}

		const int nextTileKey = TileKey(nextX, nextY);
		if (monsterTiles.find(nextTileKey) != monsterTiles.end() ||
			potionTiles.find(nextTileKey) != potionTiles.end() ||
			npcTiles.find(nextTileKey) != npcTiles.end()) {
			return false;
		}

		npcTiles.erase(TileKey(npc.x, npc.y));
		npcTiles.insert(nextTileKey);
		if (nextX < npc.x) {
			npc.direction = 1;
		}
		else if (nextX > npc.x) {
			npc.direction = 2;
		}
		else if (nextY < npc.y) {
			npc.direction = 3;
		}
		else if (nextY > npc.y) {
			npc.direction = 0;
		}
		npc.x = nextX;
		npc.y = nextY;
		return true;
	}

	bool ChasePlayer(MonsterState& monster, const ClientState& target)
	{
		const int dx = static_cast<int>(target.x) - static_cast<int>(monster.x);
		const int dy = static_cast<int>(target.y) - static_cast<int>(monster.y);
		const short stepX = dx < 0 ? -1 : 1;
		const short stepY = dy < 0 ? -1 : 1;

		if (std::abs(dx) >= std::abs(dy) && dx != 0) {
			if (TryMoveMonster(monster, static_cast<short>(monster.x + stepX), monster.y)) {
				return true;
			}
			if (dy != 0) {
				return TryMoveMonster(monster, monster.x, static_cast<short>(monster.y + stepY));
			}
		}
		else if (dy != 0) {
			if (TryMoveMonster(monster, monster.x, static_cast<short>(monster.y + stepY))) {
				return true;
			}
			if (dx != 0) {
				return TryMoveMonster(monster, static_cast<short>(monster.x + stepX), monster.y);
			}
		}

		return false;
	}

	void SyncVisibleMonsters(ClientState& client)
	{
		std::unordered_set<int> nextVisibleMonsters;

		for (const MonsterState& monster : monsters) {
			if (monster.hp <= 0) {
				continue;
			}
			if (!IsInMonsterView(client, monster)) {
				continue;
			}

			nextVisibleMonsters.insert(monster.objectId);
			if (client.visibleMonsters.find(monster.objectId) == client.visibleMonsters.end()) {
				SendAddMonster(client.socket, monster);
			}
		}

		for (int objectId : client.visibleMonsters) {
			if (nextVisibleMonsters.find(objectId) == nextVisibleMonsters.end()) {
				SendRemoveObject(client.socket, objectId);
			}
		}

		client.visibleMonsters = std::move(nextVisibleMonsters);
	}

	void SyncVisibleNpcs(ClientState& client)
	{
		std::unordered_set<int> nextVisibleNpcs;

		for (const NpcState& npc : npcs) {
			if (!IsInNpcView(client, npc)) {
				continue;
			}

			nextVisibleNpcs.insert(npc.objectId);
			if (client.visibleNpcs.find(npc.objectId) == client.visibleNpcs.end()) {
				SendAddNpc(client.socket, npc);
			}
		}

		for (int objectId : client.visibleNpcs) {
			if (nextVisibleNpcs.find(objectId) == nextVisibleNpcs.end()) {
				SendRemoveObject(client.socket, objectId);
			}
		}

		client.visibleNpcs = std::move(nextVisibleNpcs);
	}

	void MoveMonster(MonsterState& monster, std::mt19937& generator)
	{
		if (monster.hp <= 0) {
			return;
		}

		int attackDirection = 0;
		ClientState* target = FindAdjacentPlayer(monster, attackDirection);
		if (target != nullptr) {
			BroadcastMonsterAttack(monster, attackDirection);
			DamagePlayer(*target, monster.attackDamage);
			return;
		}

		if (IsBoss(monster)) {
			const auto now = std::chrono::steady_clock::now();
			int bossSkillDirection = 0;
			ClientState* skillTarget = IsBossSkillReady(monster, now)
				? FindBossSkillTarget(monster, bossSkillDirection)
				: nullptr;
			if (skillTarget != nullptr) {
				short bossSkillX = monster.x;
				short bossSkillY = monster.y;
				BossSkillStartPosition(monster, *skillTarget, bossSkillDirection, bossSkillX, bossSkillY);
				BroadcastBossSkillAttack(monster, bossSkillX, bossSkillY, bossSkillDirection);
				ResetBossSkillCooldown(monster, generator, now);
				std::thread(
					RunBossSkillDamage,
					bossSkillX,
					bossSkillY,
					bossSkillDirection,
					monster.attackDamage).detach();
				return;
			}
		}

		ClientState* chaseTarget = FindChaseTarget(monster);
		if (chaseTarget != nullptr && ChasePlayer(monster, *chaseTarget)) {
			return;
		}

		static constexpr int dx[] = { 0, -1, 1, 0, 0 };
		static constexpr int dy[] = { 0, 0, 0, -1, 1 };
		std::uniform_int_distribution<int> directionDistribution(0, 4);

		const int direction = directionDistribution(generator);
		const short nextX = static_cast<short>(monster.x + dx[direction]);
		const short nextY = static_cast<short>(monster.y + dy[direction]);
		if (direction == 0 || IsBlockedTile(nextX, nextY) || IsPlayerAt(nextX, nextY)) {
			return;
		}

		TryMoveMonster(monster, nextX, nextY);
	}

	void MoveNpc(NpcState& npc, std::mt19937& generator)
	{
		static constexpr int dx[] = { 0, -1, 1, 0, 0 };
		static constexpr int dy[] = { 0, 0, 0, -1, 1 };
		std::uniform_int_distribution<int> directionDistribution(0, 4);

		const int direction = directionDistribution(generator);
		if (direction == 0) {
			return;
		}

		TryMoveNpc(
			npc,
			static_cast<short>(npc.x + dx[direction]),
			static_cast<short>(npc.y + dy[direction]));
	}
}

void MonsterAiLoop()
{
	std::random_device randomDevice;
	std::mt19937 generator(randomDevice());

	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(MonsterMoveMilliseconds));

		std::lock_guard<std::mutex> lock(clientsMutex);
		std::unordered_set<int> activeMonsterIds;
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn) {
				continue;
			}

			for (int objectId : client->visibleMonsters) {
				activeMonsterIds.insert(objectId);
			}
		}

		for (int objectId : activeMonsterIds) {
			const int monsterIndex = objectId - NPC_ID_START;
			if (monsterIndex < 0 || monsterIndex >= static_cast<int>(monsters.size())) {
				continue;
			}

			MoveMonster(monsters[monsterIndex], generator);
		}

		for (NpcState& npc : npcs) {
			MoveNpc(npc, generator);
		}

		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn) {
				continue;
			}

			SyncVisibleMonsters(*client);
			SyncVisibleNpcs(*client);
			for (int objectId : client->visibleMonsters) {
				const int monsterIndex = objectId - NPC_ID_START;
				if (monsterIndex < 0 || monsterIndex >= static_cast<int>(monsters.size())) {
					continue;
				}

				const MonsterState& monster = monsters[monsterIndex];
				SendMoveObject(client->socket, monster.objectId, monster.x, monster.y, MonsterMoveMilliseconds);
			}
			for (int objectId : client->visibleNpcs) {
				const int npcIndex = objectId - NpcIdStart;
				if (npcIndex < 0 || npcIndex >= static_cast<int>(npcs.size())) {
					continue;
				}

				const NpcState& npc = npcs[npcIndex];
				SendMoveObject(client->socket, npc.objectId, npc.x, npc.y, MonsterMoveMilliseconds);
			}
		}
	}
}