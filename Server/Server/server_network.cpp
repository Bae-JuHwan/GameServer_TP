#include "server_network.h"

#include "database.h"
#include "packet_utils.h"
#include "protocol_2026.h"
#include "server_types.h"
#include "server_world.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_set>

namespace {
	constexpr int BufferSize = 512;
	constexpr int MonsterViewSize = 20;
	constexpr int MonsterViewRadius = MonsterViewSize / 2;
	constexpr int BossSpriteId = 16;
	constexpr int SecondBossSpriteId = 3;
	constexpr int ThirdBossSpriteId = 14;
	constexpr int FourthBossSpriteId = 9;
	constexpr int BossCollisionHalfWidthTiles = 4;
	constexpr int BossCollisionTopTiles = 7;
	constexpr int PotionIdStart = NPC_ID_START + NUM_NPCS;
	constexpr int PotionHealAmount = 20;
	constexpr int BossPotionVisualId = 2;
	constexpr int BossPotionHealAmount = 150;
	constexpr int MaxCheckedMovePathTiles = 8;
	constexpr int BeginnerQuestRequiredKills = 100;
	constexpr int IntermediateQuestRequiredKills = 100;
	constexpr int BeginnerSkillRange = 10;
	constexpr int BeginnerSkillHalfWidth = 1;
	constexpr int BeginnerSkillStepMilliseconds = 35;

	void SendRemoveObject(SOCKET clientSocket, int playerId);

	bool IsBossSprite(int spriteId)
	{
		return spriteId == BossSpriteId || spriteId == SecondBossSpriteId || spriteId == ThirdBossSpriteId || spriteId == FourthBossSpriteId;
	}

	void AppendUtf8(std::string& text, unsigned int codepoint)
	{
		if (codepoint <= 0x7F) {
			text.push_back(static_cast<char>(codepoint));
		}
		else if (codepoint <= 0x7FF) {
			text.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
			text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		}
		else if (codepoint <= 0xFFFF) {
			text.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
			text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
			text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		}
		else {
			text.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
			text.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
			text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
			text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
		}
	}

	std::string Utf8Text(std::initializer_list<unsigned int> codepoints)
	{
		std::string text;
		for (const unsigned int codepoint : codepoints) {
			AppendUtf8(text, codepoint);
		}
		return text;
	}

	std::string NpcAllRewardedText()
	{
		return Utf8Text({
			0xBAA8, 0xB4E0, 0x0020, 0xD018, 0xC2A4, 0xD2B8, 0xB97C, 0x0020,
			0xC644, 0xB8CC, 0xD588, 0xC5B4, 0x002E, 0x0020, 0xC774, 0xC81C,
			0x0020, 0xC2A4, 0xD0AC, 0xACFC, 0x0020, 0xB300, 0xC26C, 0xB97C,
			0x0020, 0xC0AC, 0xC6A9, 0xD560, 0x0020, 0xC218, 0x0020, 0xC788,
			0xC5B4, 0x002E
		});
	}

	std::string NpcSkillRewardAndNextQuestText()
	{
		return Utf8Text({
			0xD018, 0xC2A4, 0xD2B8, 0x0020, 0xC644, 0xB8CC, 0x0021, 0x0020,
			0xCD08, 0xBCF4, 0xC790, 0x0020, 0xC2A4, 0xD0AC, 0xC744, 0x0020,
			0xD68D, 0xB4DD, 0xD588, 0xC5B4, 0x002E, 0x0020, 0xB2E4, 0xC74C,
			0xC740, 0x0020, 0xC911, 0xAE09, 0xC790, 0x0020, 0xB9C8, 0xC744,
			0xC5D0, 0xC11C, 0x0020, 0xBAAC, 0xC2A4, 0xD130, 0x0020, 0x0031,
			0x0030, 0x0030, 0xB9C8, 0xB9AC, 0x0020, 0xC7A1, 0xAE30, 0xC57C,
			0x002E
		});
	}

	std::string NpcQuestText()
	{
		return Utf8Text({
			0xD018, 0xC2A4, 0xD2B8, 0x003A, 0x0020, 0xCD08, 0xBCF4, 0xC790, 0x0020,
			0xB9C8, 0xC744, 0xC5D0, 0xC11C, 0x0020, 0xBAAC, 0xC2A4, 0xD130, 0x0020,
			0x0031, 0x0030, 0x0030, 0xB9C8, 0xB9AC, 0xB97C, 0x0020, 0xC7A1, 0xC544,
			0xC918, 0x002E, 0x0020, 0xBCF4, 0xC0C1, 0xC740, 0x0020, 0xC2A4, 0xD0AC,
			0xC774, 0xC57C, 0x002E
		});
	}

	std::string NpcIntermediateQuestText()
	{
		return Utf8Text({
			0xD018, 0xC2A4, 0xD2B8, 0x003A, 0x0020, 0xC911, 0xAE09, 0xC790,
			0x0020, 0xB9C8, 0xC744, 0xC5D0, 0xC11C, 0x0020, 0xBAAC, 0xC2A4,
			0xD130, 0x0020, 0x0031, 0x0030, 0x0030, 0xB9C8, 0xB9AC, 0xB97C,
			0x0020, 0xC7A1, 0xC544, 0xC640, 0x002E, 0x0020, 0xBCF4, 0xC0C1,
			0xC740, 0x0020, 0xB300, 0xC26C, 0xC57C, 0x002E
		});
	}

	std::string NpcDashRewardText()
	{
		return Utf8Text({
			0xD018, 0xC2A4, 0xD2B8, 0x0020, 0xC644, 0xB8CC, 0x0021, 0x0020,
			0xC774, 0xC81C, 0x0020, 0x0053, 0x0068, 0x0069, 0x0066, 0x0074,
			0x0020, 0xD0A4, 0xB85C, 0x0020, 0xB300, 0xC26C, 0xD560, 0x0020,
			0xC218, 0x0020, 0xC788, 0xC5B4, 0x002E
		});
	}

	std::string NpcQuestProgressText(int kills)
	{
		return Utf8Text({ 0xC9C4, 0xD589, 0x0020, 0xC911, 0x003A, 0x0020 }) +
			std::to_string(kills) +
			Utf8Text({ 0x002F, 0x0031, 0x0030, 0x0030, 0x0020, 0xB9C8, 0xB9AC });
	}

	std::string NpcIntermediateQuestProgressText(int kills)
	{
		return Utf8Text({
			0xC911, 0xAE09, 0x0020, 0xD018, 0xC2A4, 0xD2B8, 0x0020,
			0xC9C4, 0xD589, 0x0020, 0xC911, 0x003A, 0x0020
		}) + std::to_string(kills) + Utf8Text({
			0x002F, 0x0031, 0x0030, 0x0030, 0x0020, 0xB9C8, 0xB9AC
		});
	}

	void SendLoginResult(SOCKET clientSocket, bool success, const std::string& message)
	{
		S2C_LoginResult packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_LOGIN_RESULT;
		packet.success = success;
		CopyText(packet.message, sizeof(packet.message), message);
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendAvatarInfo(SOCKET clientSocket, const ClientState& player)
	{
		S2C_AvatarInfo packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_AVATAR_INFO;
		packet.playerId = player.playerId;
		packet.visualId = 10;
		packet.x = player.x;
		packet.y = player.y;
		packet.hp = player.hp;
		packet.max_hp = player.maxHp;
		packet.exp = player.exp;
		packet.level = player.level;
		packet.has_dash = player.hasDash;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendMoveObject(SOCKET clientSocket, int playerId, short x, short y, int moveTime)
	{
		S2C_MoveObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_MOVE_OBJECT;
		packet.object_id = playerId;
		packet.x = x;
		packet.y = y;
		packet.move_time = moveTime;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendStatusChange(SOCKET clientSocket, int objectId, int hp, int maxHp, unsigned long long exp, unsigned char level, bool hasDash)
	{
		S2C_StatusChange packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_STATUS_CHANGE;
		packet.object_id = objectId;
		packet.hp = hp;
		packet.max_hp = maxHp;
		packet.exp = exp;
		packet.level = level;
		packet.has_dash = hasDash;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendNpcDialogue(SOCKET clientSocket, int objectId, const std::string& message)
	{
		S2C_NpcDialogue packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_NPC_DIALOGUE;
		packet.object_id = objectId;
		CopyText(packet.message, sizeof(packet.message), message);
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendSkillObject(SOCKET clientSocket, int objectId, short x, short y, int direction)
	{
		S2C_SkillObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_SKILL_OBJECT;
		packet.object_id = objectId;
		packet.visual_id = 0;
		packet.x = x;
		packet.y = y;
		packet.direction = direction;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendAddObject(SOCKET clientSocket, const ClientState& player)
	{
		S2C_AddObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_ADD_OBJECT;
		packet.object_id = player.playerId;
		packet.visual_id = 10;
		CopyText(packet.obj_name, sizeof(packet.obj_name), player.name);
		packet.x = player.x;
		packet.y = player.y;
		packet.hp = player.hp;
		packet.max_hp = player.maxHp;
		packet.exp = player.exp;
		packet.level = player.level;
		SendAll(clientSocket, &packet, sizeof(packet));
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

	void SendAddPotion(SOCKET clientSocket, const PotionState& potion)
	{
		S2C_AddObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_ADD_OBJECT;
		packet.object_id = potion.objectId;
		packet.visual_id = potion.visualId;
		CopyText(packet.obj_name, sizeof(packet.obj_name), "Potion");
		packet.x = potion.x;
		packet.y = potion.y;
		packet.hp = 0;
		packet.max_hp = 0;
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

	bool IsInMonsterView(const ClientState& client, const MonsterState& monster)
	{
		const int viewRadius = MonsterViewRadius + (IsBossSprite(monster.spriteId) ? BossCollisionTopTiles : 0);
		return std::abs(static_cast<int>(monster.x) - static_cast<int>(client.x)) <= viewRadius &&
			std::abs(static_cast<int>(monster.y) - static_cast<int>(client.y)) <= viewRadius;
	}

	bool IsInMonsterCollision(const MonsterState& monster, short x, short y)
	{
		if (monster.hp <= 0) {
			return false;
		}

		if (IsBossSprite(monster.spriteId)) {
			return std::abs(static_cast<int>(monster.x) - static_cast<int>(x)) <= BossCollisionHalfWidthTiles &&
				static_cast<int>(y) >= static_cast<int>(monster.y) - BossCollisionTopTiles &&
				static_cast<int>(y) <= static_cast<int>(monster.y);
		}

		return monster.x == x && monster.y == y;
	}

	bool IsInPotionView(const ClientState& client, const PotionState& potion)
	{
		return std::abs(static_cast<int>(potion.x) - static_cast<int>(client.x)) <= MonsterViewRadius &&
			std::abs(static_cast<int>(potion.y) - static_cast<int>(client.y)) <= MonsterViewRadius;
	}

	bool IsInNpcView(const ClientState& client, const NpcState& npc)
	{
		return std::abs(static_cast<int>(npc.x) - static_cast<int>(client.x)) <= MonsterViewRadius &&
			std::abs(static_cast<int>(npc.y) - static_cast<int>(client.y)) <= MonsterViewRadius;
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

	void SyncVisiblePotions(ClientState& client)
	{
		std::unordered_set<int> nextVisiblePotions;

		for (const PotionState& potion : potions) {
			if (!potion.active || !IsInPotionView(client, potion)) {
				continue;
			}

			nextVisiblePotions.insert(potion.objectId);
			if (client.visiblePotions.find(potion.objectId) == client.visiblePotions.end()) {
				SendAddPotion(client.socket, potion);
			}
		}

		for (int objectId : client.visiblePotions) {
			if (nextVisiblePotions.find(objectId) == nextVisiblePotions.end()) {
				SendRemoveObject(client.socket, objectId);
			}
		}

		client.visiblePotions = std::move(nextVisiblePotions);
	}

	void BroadcastAddPotion(const PotionState& potion)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn || !IsInPotionView(*client, potion)) {
				continue;
			}

			SendAddPotion(client->socket, potion);
			client->visiblePotions.insert(potion.objectId);
		}
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

	bool IsMonsterAt(short x, short y)
	{
		if (monsterTiles.find(TileKey(x, y)) != monsterTiles.end()) {
			return true;
		}

		for (const MonsterState& monster : monsters) {
			if (monster.hp <= 0 || !IsBossSprite(monster.spriteId)) {
				continue;
			}

			if (IsInMonsterCollision(monster, x, y)) {
				return true;
			}
		}

		return false;
	}

	bool IsNpcAt(short x, short y)
	{
		return npcTiles.find(TileKey(x, y)) != npcTiles.end();
	}

	bool IsPlayerAt(short x, short y, int ignoredPlayerId)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (!client->loggedIn || client->playerId == ignoredPlayerId || client->hp <= 0) {
				continue;
			}

			if (client->x == x && client->y == y) {
				return true;
			}
		}

		return false;
	}

	bool IsMovePathBlocked(short startX, short startY, short endX, short endY)
	{
		const int deltaX = static_cast<int>(endX) - static_cast<int>(startX);
		const int deltaY = static_cast<int>(endY) - static_cast<int>(startY);
		if (deltaX != 0 && deltaY != 0) {
			return IsMonsterAt(endX, endY) || IsNpcAt(endX, endY);
		}

		const int distance = std::abs(deltaX) + std::abs(deltaY);
		if (distance > MaxCheckedMovePathTiles) {
			return IsMonsterAt(endX, endY) || IsNpcAt(endX, endY);
		}

		const int stepX = deltaX == 0 ? 0 : (deltaX < 0 ? -1 : 1);
		const int stepY = deltaY == 0 ? 0 : (deltaY < 0 ? -1 : 1);
		for (int step = 1; step <= distance; ++step) {
			const short checkX = static_cast<short>(startX + stepX * step);
			const short checkY = static_cast<short>(startY + stepY * step);
			if (IsMonsterAt(checkX, checkY) || IsNpcAt(checkX, checkY)) {
				return true;
			}
		}

		return false;
	}

	bool IsPlayerMovePathBlocked(short startX, short startY, short endX, short endY, int ignoredPlayerId)
	{
		const int deltaX = static_cast<int>(endX) - static_cast<int>(startX);
		const int deltaY = static_cast<int>(endY) - static_cast<int>(startY);
		if (deltaX != 0 && deltaY != 0) {
			return IsPlayerAt(endX, endY, ignoredPlayerId);
		}

		const int distance = std::abs(deltaX) + std::abs(deltaY);
		if (distance > MaxCheckedMovePathTiles) {
			return IsPlayerAt(endX, endY, ignoredPlayerId);
		}

		const int stepX = deltaX == 0 ? 0 : (deltaX < 0 ? -1 : 1);
		const int stepY = deltaY == 0 ? 0 : (deltaY < 0 ? -1 : 1);
		for (int step = 1; step <= distance; ++step) {
			const short checkX = static_cast<short>(startX + stepX * step);
			const short checkY = static_cast<short>(startY + stepY * step);
			if (IsPlayerAt(checkX, checkY, ignoredPlayerId)) {
				return true;
			}
		}

		return false;
	}

	NpcState* FindNpcById(int objectId)
	{
		for (NpcState& npc : npcs) {
			if (npc.objectId == objectId) {
				return &npc;
			}
		}

		return nullptr;
	}

	bool IsNearNpc(const ClientState& player, const NpcState& npc)
	{
		return std::abs(static_cast<int>(player.x) - static_cast<int>(npc.x)) <= 1 &&
			std::abs(static_cast<int>(player.y) - static_cast<int>(npc.y)) <= 1;
	}

	bool IsInMapSection(short x, short y, int mapNumber)
	{
		int sectionX = 0;
		int sectionY = 0;
		if (!SectionFromMapNumber(mapNumber, sectionX, sectionY)) {
			return false;
		}

		const int minX = WORLD_WIDTH * sectionX / 3;
		const int maxX = WORLD_WIDTH * (sectionX + 1) / 3 - 1;
		const int minY = WORLD_HEIGHT * sectionY / 3;
		const int maxY = WORLD_HEIGHT * (sectionY + 1) / 3 - 1;
		return x >= minX && x <= maxX && y >= minY && y <= maxY;
	}

	bool IsBeginnerMonsterPosition(short x, short y)
	{
		return IsInMapSection(x, y, 1) || IsInMapSection(x, y, 3);
	}

	bool IsIntermediateMonsterPosition(short x, short y)
	{
		return IsInMapSection(x, y, 2);
	}

	void ProgressBeginnerQuest(ClientState& player, const MonsterState& monster)
	{
		if (!player.beginnerQuestAccepted || player.beginnerQuestComplete || !IsBeginnerMonsterPosition(monster.x, monster.y)) {
			return;
		}

		player.beginnerQuestKills = std::min(BeginnerQuestRequiredKills, player.beginnerQuestKills + 1);
		if (player.beginnerQuestKills >= BeginnerQuestRequiredKills) {
			player.beginnerQuestComplete = true;
		}
	}

	void ProgressIntermediateQuest(ClientState& player, const MonsterState& monster)
	{
		if (!player.intermediateQuestAccepted || player.intermediateQuestComplete ||
			!IsIntermediateMonsterPosition(monster.x, monster.y)) {
			return;
		}

		player.intermediateQuestKills = std::min(IntermediateQuestRequiredKills, player.intermediateQuestKills + 1);
		if (player.intermediateQuestKills >= IntermediateQuestRequiredKills) {
			player.intermediateQuestComplete = true;
		}
	}

	void HandleNpcInteraction(ClientState& player, int objectId)
	{
		NpcState* npc = FindNpcById(objectId);
		if (npc == nullptr || !IsNearNpc(player, *npc)) {
			return;
		}

		if (player.hasDash) {
			SendNpcDialogue(player.socket, npc->objectId, NpcAllRewardedText());
			return;
		}
		if (!player.beginnerQuestAccepted) {
			player.beginnerQuestAccepted = true;
			player.beginnerQuestKills = 0;
			SendNpcDialogue(player.socket, npc->objectId, NpcQuestText());
			return;
		}
		if (!player.beginnerQuestComplete) {
			SendNpcDialogue(
				player.socket,
				npc->objectId,
				NpcQuestProgressText(player.beginnerQuestKills));
			return;
		}
		if (!player.hasBeginnerSkill) {
			player.hasBeginnerSkill = true;
			if (!player.intermediateQuestAccepted) {
				player.intermediateQuestAccepted = true;
				player.intermediateQuestKills = 0;
			}
			SendStatusChange(player.socket, player.playerId, player.hp, player.maxHp, player.exp, player.level, player.hasDash);
			SendNpcDialogue(player.socket, npc->objectId, NpcSkillRewardAndNextQuestText());
			return;
		}
		if (!player.intermediateQuestAccepted) {
			player.intermediateQuestAccepted = true;
			player.intermediateQuestKills = 0;
			SendNpcDialogue(player.socket, npc->objectId, NpcIntermediateQuestText());
			return;
		}
		if (!player.intermediateQuestComplete) {
			SendNpcDialogue(
				player.socket,
				npc->objectId,
				NpcIntermediateQuestProgressText(player.intermediateQuestKills));
			return;
		}

		player.hasDash = true;
		SendStatusChange(player.socket, player.playerId, player.hp, player.maxHp, player.exp, player.level, player.hasDash);
		SendNpcDialogue(player.socket, npc->objectId, NpcDashRewardText());
	}

	void SendRemoveObject(SOCKET clientSocket, int playerId)
	{
		S2C_RemoveObject packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_REMOVE_OBJECT;
		packet.object_id = playerId;
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void SendChatMessage(SOCKET clientSocket, int playerId, const std::string& message)
	{
		S2C_ChatMessage packet{};
		packet.size = sizeof(packet);
		packet.type = S2C_CHAT_MESSAGE;
		packet.object_id = playerId;
		CopyText(packet.message, sizeof(packet.message), message);
		SendAll(clientSocket, &packet, sizeof(packet));
	}

	void BroadcastAddObject(const ClientState& player)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && client->playerId != player.playerId) {
				SendAddObject(client->socket, player);
			}
		}
	}

	void BroadcastMoveObject(int playerId, short x, short y, int moveTime)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn) {
				SendMoveObject(client->socket, playerId, x, y, moveTime);
			}
		}
	}

	void BroadcastRemoveObject(int playerId)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && client->playerId != playerId) {
				SendRemoveObject(client->socket, playerId);
			}
		}
	}

	void BroadcastChatMessage(int playerId, const std::string& message)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn) {
				SendChatMessage(client->socket, playerId, message);
			}
		}
	}

	void BroadcastStatusChange(int objectId, int hp, int maxHp, unsigned long long exp, unsigned char level, bool hasDash)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn) {
				SendStatusChange(client->socket, objectId, hp, maxHp, exp, level, hasDash);
			}
		}
	}

	void BroadcastSkillObject(const ClientState& player, int direction)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn) {
				SendSkillObject(client->socket, player.playerId, player.x, player.y, direction);
			}
		}
	}

	void AddPlayerExp(ClientState& player, unsigned long long exp)
	{
		player.exp += exp;
		while (player.exp >= RequiredExpForNextLevel(player.level)) {
			player.exp -= RequiredExpForNextLevel(player.level);
			++player.level;
		}

		if (player.level >= PlayerSecondPowerUpLevel) {
			const bool shouldHeal = player.maxHp < PlayerSecondPowerUpHp;
			player.maxHp = PlayerSecondPowerUpHp;
			player.attackDamage = PlayerSecondPowerUpAttackDamage;
			if (shouldHeal) {
				player.hp = player.maxHp;
			}
		}
		else if (player.level >= PlayerPowerUpLevel) {
			const bool shouldHeal = player.maxHp < PlayerPowerUpHp;
			player.maxHp = PlayerPowerUpHp;
			player.attackDamage = PlayerPowerUpAttackDamage;
			if (shouldHeal) {
				player.hp = player.maxHp;
			}
		}
	}

	void SavePlayerIfConnected(const ClientState& player)
	{
		if (g_db.is_connected()) {
			g_db.save_player(player);
		}
	}

	bool AttackDirectionOffset(int direction, short& dx, short& dy)
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
		case 4:
			dx = -1;
			dy = -1;
			return true;
		case 5:
			dx = 1;
			dy = -1;
			return true;
		case 6:
			dx = -1;
			dy = 1;
			return true;
		case 7:
			dx = 1;
			dy = 1;
			return true;
		default:
			dy = 1;
			return true;
		}
	}

	MonsterState* FindMonsterAt(short x, short y)
	{
		for (MonsterState& monster : monsters) {
			if (IsInMonsterCollision(monster, x, y)) {
				return &monster;
			}
		}

		return nullptr;
	}

	PotionState* FindPotionAt(short x, short y)
	{
		for (PotionState& potion : potions) {
			if (potion.active && potion.x == x && potion.y == y) {
				return &potion;
			}
		}

		return nullptr;
	}

	bool TryConsumePotion(ClientState& player)
	{
		PotionState* potion = FindPotionAt(player.x, player.y);
		if (potion == nullptr) {
			return false;
		}

		const int oldHp = player.hp;
		player.hp = std::min(player.maxHp, player.hp + potion->healAmount);
		potion->active = false;
		potionTiles.erase(TileKey(potion->x, potion->y));
		BroadcastRemoveObject(potion->objectId);
		return player.hp != oldHp;
	}

	void DropPotionAt(short x, short y)
	{
		const int tileKey = TileKey(x, y);
		if (PotionState* existingPotion = FindPotionAt(x, y)) {
			existingPotion->active = false;
			potionTiles.erase(tileKey);
			BroadcastRemoveObject(existingPotion->objectId);
		}

		PotionState potion{};
		potion.objectId = PotionIdStart + static_cast<int>(potions.size());
		potion.x = x;
		potion.y = y;
		potion.visualId = BossPotionVisualId;
		potion.healAmount = BossPotionHealAmount;
		potion.active = true;

		potions.push_back(potion);
		potionTiles.insert(tileKey);
		BroadcastAddPotion(potion);
	}

	bool DamageMonsterAt(ClientState& player, short x, short y, std::unordered_set<int>* damagedMonsterIds = nullptr)
	{
		MonsterState* monster = FindMonsterAt(x, y);
		if (monster == nullptr) {
			return false;
		}

		if (damagedMonsterIds != nullptr) {
			if (damagedMonsterIds->find(monster->objectId) != damagedMonsterIds->end()) {
				return false;
			}
			damagedMonsterIds->insert(monster->objectId);
		}

		monster->hp = std::max(0, monster->hp - player.attackDamage);
		BroadcastStatusChange(monster->objectId, monster->hp, monster->maxHp, 0, 1, false);
		if (monster->hp <= 0) {
			const bool isBoss = IsBossSprite(monster->spriteId);
			ProgressBeginnerQuest(player, *monster);
			ProgressIntermediateQuest(player, *monster);
			monsterTiles.erase(TileKey(monster->x, monster->y));
			BroadcastRemoveObject(monster->objectId);
			if (isBoss) {
				DropPotionAt(monster->x, monster->y);
			}
			AddPlayerExp(player, monster->expReward);
			return true;
		}

		return false;
	}

	ClientState* FindLoggedInPlayerById(int playerId)
	{
		for (const std::shared_ptr<ClientState>& client : clients) {
			if (client->loggedIn && client->playerId == playerId) {
				return client.get();
			}
		}

		return nullptr;
	}

	void ApplyBeginnerSkillDamageStep(
		int playerId,
		short startX,
		short startY,
		int direction,
		int distance,
		std::unordered_set<int>& damagedMonsterIds)
	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		ClientState* player = FindLoggedInPlayerById(playerId);
		if (player == nullptr) {
			return;
		}

		short dx = 0;
		short dy = 0;
		AttackDirectionOffset(direction, dx, dy);

		bool gainedExp = false;
		for (int widthOffset = -BeginnerSkillHalfWidth; widthOffset <= BeginnerSkillHalfWidth; ++widthOffset) {
			const short skillX = static_cast<short>(startX + dx * distance + (dy == 0 ? 0 : widthOffset));
			const short skillY = static_cast<short>(startY + dy * distance + (dx == 0 ? 0 : widthOffset));
			if (DamageMonsterAt(*player, skillX, skillY, &damagedMonsterIds)) {
				gainedExp = true;
			}
		}

		if (gainedExp) {
			SavePlayerIfConnected(*player);
			BroadcastStatusChange(player->playerId, player->hp, player->maxHp, player->exp, player->level, player->hasDash);
		}
	}

	void RunBeginnerSkillDamage(int playerId, short startX, short startY, int direction)
	{
		std::unordered_set<int> damagedMonsterIds;
		for (int distance = 1; distance <= BeginnerSkillRange; ++distance) {
			std::this_thread::sleep_for(std::chrono::milliseconds(BeginnerSkillStepMilliseconds));
			ApplyBeginnerSkillDamageStep(playerId, startX, startY, direction, distance, damagedMonsterIds);
		}
	}

	bool CastBeginnerSkill(ClientState& player, int direction)
	{
		if (!player.hasBeginnerSkill) {
			return false;
		}

		BroadcastSkillObject(player, direction);
		std::thread(
			RunBeginnerSkillDamage,
			player.playerId,
			player.x,
			player.y,
			direction).detach();
		return true;
	}

	void RemoveClient(const std::shared_ptr<ClientState>& state)
	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		clients.erase(
			std::remove_if(clients.begin(), clients.end(), [&](const std::shared_ptr<ClientState>& client) {
				return client->playerId == state->playerId;
			}),
			clients.end());
	}
}

void HandleClient(SOCKET clientSocket, int playerId)
{
	char buffer[BufferSize]{};
	std::shared_ptr<ClientState> state = std::make_shared<ClientState>();
	state->socket = clientSocket;
	state->playerId = playerId;
	RandomOpenPosition(state->x, state->y);
	for (int attempt = 0; attempt < 1000 && (IsMonsterAt(state->x, state->y) || IsNpcAt(state->x, state->y)); ++attempt) {
		RandomOpenPosition(state->x, state->y);
	}

	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		clients.push_back(state);
	}

	std::cout << "Client connected. playerId=" << playerId
		<< " startX=" << state->x << " startY=" << state->y << '\n';

	while (true) {
		const int received = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (received <= 0) {
			break;
		}

		int offset = 0;
		while (offset < received) {
			const unsigned char packetSize = static_cast<unsigned char>(buffer[offset]);
			if (packetSize == 0 || offset + packetSize > received) {
				break;
			}

			const PACKET_TYPE type = *reinterpret_cast<PACKET_TYPE*>(buffer + offset + 1);

			switch (type) {
			case C2S_LOGIN: {
				const C2S_Login* packet = reinterpret_cast<const C2S_Login*>(buffer + offset);
				state->name = PacketText(packet->username, MAX_NAME_LEN);
				if (g_db.is_connected()) {
					if (g_db.load_player(state->name.c_str(), *state)) {
						if (IsBlockedTile(state->x, state->y) || IsMonsterAt(state->x, state->y) || IsNpcAt(state->x, state->y)) {
							RandomOpenPosition(state->x, state->y);
							for (int attempt = 0; attempt < 1000 && (IsMonsterAt(state->x, state->y) || IsNpcAt(state->x, state->y)); ++attempt) {
								RandomOpenPosition(state->x, state->y);
							}
							SavePlayerIfConnected(*state);
						}
					}
					else {
						SavePlayerIfConnected(*state);
					}
				}
				bool loginPositionChanged = false;
				state->loggedIn = true;
				std::cout << "Login playerName=" << state->name << " internalPlayerId=" << playerId << '\n';

				{
					std::lock_guard<std::mutex> lock(clientsMutex);
					if (IsPlayerAt(state->x, state->y, state->playerId)) {
						RandomOpenPosition(state->x, state->y);
						for (int attempt = 0; attempt < 1000 &&
							(IsMonsterAt(state->x, state->y) || IsNpcAt(state->x, state->y) ||
								IsPlayerAt(state->x, state->y, state->playerId)); ++attempt) {
							RandomOpenPosition(state->x, state->y);
						}
						loginPositionChanged = true;
					}

					SendLoginResult(clientSocket, true, "Login success");
					SendAvatarInfo(clientSocket, *state);

					for (const std::shared_ptr<ClientState>& client : clients) {
						if (client->loggedIn && client->playerId != state->playerId) {
							SendAddObject(clientSocket, *client);
						}
					}
					SyncVisibleMonsters(*state);
					SyncVisiblePotions(*state);
					SyncVisibleNpcs(*state);
					BroadcastAddObject(*state);
				}

				if (loginPositionChanged) {
					SavePlayerIfConnected(*state);
				}
				break;
			}
			case C2S_MOVE: {
				const C2S_Move* packet = reinterpret_cast<const C2S_Move*>(buffer + offset);
				if (IsMovePathBlocked(state->x, state->y, packet->x, packet->y)) {
					SendMoveObject(clientSocket, playerId, state->x, state->y, 0);
					break;
				}

				{
					std::lock_guard<std::mutex> lock(clientsMutex);
					if (IsPlayerMovePathBlocked(state->x, state->y, packet->x, packet->y, state->playerId)) {
						SendMoveObject(clientSocket, playerId, state->x, state->y, 0);
						break;
					}

					state->x = packet->x;
					state->y = packet->y;
					BroadcastMoveObject(playerId, state->x, state->y, packet->move_time);
					if (TryConsumePotion(*state)) {
						BroadcastStatusChange(state->playerId, state->hp, state->maxHp, state->exp, state->level, state->hasDash);
					}
					SyncVisibleMonsters(*state);
					SyncVisiblePotions(*state);
					SyncVisibleNpcs(*state);
				}
				SavePlayerIfConnected(*state);
				break;
			}
			case C2S_CHAT: {
				const C2S_Chat* packet = reinterpret_cast<const C2S_Chat*>(buffer + offset);
				const std::string message = PacketText(packet->message, MAX_CHAT_MSG_LEN);
				if (!message.empty() && state->loggedIn) {
					std::lock_guard<std::mutex> lock(clientsMutex);
					BroadcastChatMessage(playerId, state->name + ": " + message);
				}
				break;
			}
			case C2S_ATTACK: {
				const C2S_Attack* packet = reinterpret_cast<const C2S_Attack*>(buffer + offset);
				std::lock_guard<std::mutex> lock(clientsMutex);
				bool gainedExp = false;
				std::unordered_set<int> damagedMonsterIds;

				const int attackDirectionCount = state->level >= PlayerSecondPowerUpLevel ? 8 : 4;
				for (int attackDirection = 0; attackDirection < attackDirectionCount; ++attackDirection) {
					if (state->level < PlayerPowerUpLevel && attackDirection != packet->direction) {
						continue;
					}

					short dx = 0;
					short dy = 0;
					AttackDirectionOffset(attackDirection, dx, dy);
					for (int distance = 1; distance <= 2; ++distance) {
						const short attackX = static_cast<short>(state->x + dx * distance);
						const short attackY = static_cast<short>(state->y + dy * distance);
						if (DamageMonsterAt(*state, attackX, attackY, &damagedMonsterIds)) {
							gainedExp = true;
						}
					}
				}
				if (gainedExp) {
					SavePlayerIfConnected(*state);
					BroadcastStatusChange(state->playerId, state->hp, state->maxHp, state->exp, state->level, state->hasDash);
				}
				break;
			}
			case C2S_INTERACT: {
				const C2S_Interact* packet = reinterpret_cast<const C2S_Interact*>(buffer + offset);
				std::lock_guard<std::mutex> lock(clientsMutex);
				HandleNpcInteraction(*state, packet->object_id);
				SavePlayerIfConnected(*state);
				break;
			}
			case C2S_SKILL: {
				const C2S_Skill* packet = reinterpret_cast<const C2S_Skill*>(buffer + offset);
				std::lock_guard<std::mutex> lock(clientsMutex);
				CastBeginnerSkill(*state, packet->direction);
				break;
			}
			case C2S_LOGOUT:
				std::cout << "Logout playerId=" << playerId << '\n';
				if (state->loggedIn) {
					SavePlayerIfConnected(*state);
					std::lock_guard<std::mutex> lock(clientsMutex);
					BroadcastRemoveObject(playerId);
				}
				RemoveClient(state);
				closesocket(clientSocket);
				return;
			default:
				std::cout << "Unhandled packet type: " << type << '\n';
				break;
			}

			offset += packetSize;
		}
	}

	std::cout << "Client disconnected. playerId=" << playerId << '\n';
	if (state->loggedIn) {
		SavePlayerIfConnected(*state);
		std::lock_guard<std::mutex> lock(clientsMutex);
		BroadcastRemoveObject(playerId);
	}
	RemoveClient(state);
	closesocket(clientSocket);
}