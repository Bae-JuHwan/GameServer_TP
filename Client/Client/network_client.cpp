#include "network_client.h"

#include <ws2tcpip.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

#include "client_config.h"

#pragma comment(lib, "ws2_32.lib")

namespace {
	void CopyText(char* destination, int destinationSize, const std::string& text)
	{
		std::memset(destination, 0, destinationSize);
		const int copySize = std::min(destinationSize - 1, static_cast<int>(text.size()));
		std::memcpy(destination, text.c_str(), copySize);
	}

	std::string PacketText(const char* text, int maxSize)
	{
		int size = 0;
		while (size < maxSize && text[size] != '\0') {
			++size;
		}

		return std::string(text, size);
	}
}

NetworkClient::~NetworkClient()
{
	SendLogout();
	if (socket_ != INVALID_SOCKET) {
		closesocket(socket_);
	}
	if (wsaStarted_) {
		WSACleanup();
	}
}

bool NetworkClient::Connect(const char* address, const char* username)
{
	if (WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0) {
		std::cout << "Network disabled: WSAStartup failed\n";
		return false;
	}
	wsaStarted_ = true;

	socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket_ == INVALID_SOCKET) {
		std::cout << "Network disabled: socket failed\n";
		return false;
	}

	sockaddr_in serverAddress{};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT);
	inet_pton(AF_INET, address, &serverAddress.sin_addr);

	if (connect(socket_, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
		std::cout << "Server connection failed. Running local-only mode.\n";
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
		return false;
	}

	u_long nonBlocking = 1;
	ioctlsocket(socket_, FIONBIO, &nonBlocking);
	connected_ = true;
	std::cout << "Connected to server " << address << ':' << PORT << '\n';
	SendLogin(username);
	return true;
}

bool NetworkClient::IsConnected() const
{
	return connected_;
}

void NetworkClient::ProcessPackets(
	int& playerX,
	int& playerY,
	int& playerHp,
	int& playerMaxHp,
	unsigned long long& playerExp,
	unsigned char& playerLevel,
	bool& playerCanDash,
	std::unordered_map<int, RemotePlayer>& remotePlayers,
	std::unordered_map<int, Monster>& monsters,
	std::unordered_map<int, Potion>& potions,
	std::unordered_map<int, Npc>& npcs,
	std::vector<ChatMessage>& chatMessages,
	std::vector<SkillEffect>& skillEffects)
{
	if (!connected_) {
		return;
	}

	char buffer[512]{};
	while (true) {
		const int received = recv(socket_, buffer, sizeof(buffer), 0);
		if (received > 0) {
			receiveBuffer_.insert(receiveBuffer_.end(), buffer, buffer + received);
			continue;
		}

		if (received == 0) {
			Disconnect("Server disconnected");
			return;
		}

		const int error = WSAGetLastError();
		if (error != WSAEWOULDBLOCK) {
			Disconnect("Server receive error");
			return;
		}
		break;
	}

	while (!receiveBuffer_.empty()) {
		const unsigned char packetSize = static_cast<unsigned char>(receiveBuffer_[0]);
		if (packetSize == 0) {
			Disconnect("Invalid packet size");
			return;
		}
		if (receiveBuffer_.size() < packetSize) {
			break;
		}

		PACKET_TYPE type{};
		std::memcpy(&type, receiveBuffer_.data() + 1, sizeof(type));
		HandlePacket(type, receiveBuffer_.data(), playerX, playerY, playerHp, playerMaxHp, playerExp, playerLevel, playerCanDash, remotePlayers, monsters, potions, npcs, chatMessages, skillEffects);
		receiveBuffer_.erase(receiveBuffer_.begin(), receiveBuffer_.begin() + packetSize);
	}
}

void NetworkClient::SendMove(short x, short y, int moveTime)
{
	if (!connected_) {
		return;
	}

	C2S_Move packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_MOVE;
	packet.x = x;
	packet.y = y;
	packet.move_time = moveTime;
	SendAll(&packet, sizeof(packet));
}

void NetworkClient::SendChat(const std::string& message)
{
	if (!connected_ || message.empty()) {
		return;
	}

	C2S_Chat packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_CHAT;
	CopyText(packet.message, sizeof(packet.message), message);
	SendAll(&packet, sizeof(packet));
}

void NetworkClient::SendAttack(int direction)
{
	if (!connected_) {
		return;
	}

	C2S_Attack packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_ATTACK;
	packet.direction = direction;
	SendAll(&packet, sizeof(packet));
}

void NetworkClient::SendInteract(int objectId)
{
	if (!connected_) {
		return;
	}

	C2S_Interact packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_INTERACT;
	packet.object_id = objectId;
	SendAll(&packet, sizeof(packet));
}

void NetworkClient::SendSkill(int direction)
{
	if (!connected_) {
		return;
	}

	C2S_Skill packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_SKILL;
	packet.direction = direction;
	SendAll(&packet, sizeof(packet));
}

void NetworkClient::SendLogin(const char* username)
{
	C2S_Login packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_LOGIN;
	CopyText(packet.username, sizeof(packet.username), username);
	SendAll(&packet, sizeof(packet));
}

void NetworkClient::SendLogout()
{
	if (!connected_) {
		return;
	}

	C2S_Logout packet{};
	packet.size = sizeof(packet);
	packet.type = C2S_LOGOUT;
	SendAll(&packet, sizeof(packet));
	connected_ = false;
}

bool NetworkClient::SendAll(const void* data, int size)
{
	const char* bytes = static_cast<const char*>(data);
	int sent = 0;

	while (sent < size) {
		const int result = send(socket_, bytes + sent, size - sent, 0);
		if (result == SOCKET_ERROR || result == 0) {
			Disconnect("Server send error");
			return false;
		}
		sent += result;
	}

	return true;
}

void NetworkClient::HandlePacket(
	PACKET_TYPE type,
	const char* data,
	int& playerX,
	int& playerY,
	int& playerHp,
	int& playerMaxHp,
	unsigned long long& playerExp,
	unsigned char& playerLevel,
	bool& playerCanDash,
	std::unordered_map<int, RemotePlayer>& remotePlayers,
	std::unordered_map<int, Monster>& monsters,
	std::unordered_map<int, Potion>& potions,
	std::unordered_map<int, Npc>& npcs,
	std::vector<ChatMessage>& chatMessages,
	std::vector<SkillEffect>& skillEffects)
{
	switch (type) {
	case S2C_LOGIN_RESULT: {
		const S2C_LoginResult* packet = reinterpret_cast<const S2C_LoginResult*>(data);
		break;
	}
	case S2C_AVATAR_INFO: {
		const S2C_AvatarInfo* packet = reinterpret_cast<const S2C_AvatarInfo*>(data);
		playerId_ = packet->playerId;
		playerX = packet->x;
		playerY = packet->y;
		playerHp = packet->hp;
		playerMaxHp = packet->max_hp;
		playerExp = packet->exp;
		playerLevel = packet->level;
		playerCanDash = packet->has_dash;
		std::cout << "Avatar playerId=" << playerId_ << " x=" << playerX << " y=" << playerY << '\n';
		break;
	}
	case S2C_ADD_OBJECT: {
		const S2C_AddObject* packet = reinterpret_cast<const S2C_AddObject*>(data);
		const std::string objectName = PacketText(packet->obj_name, MAX_NAME_LEN);
		if (objectName == "Potion") {
			Potion& potion = potions[packet->object_id];
			potion.objectId = packet->object_id;
			potion.x = packet->x;
			potion.y = packet->y;
			potion.visualId = packet->visual_id;
		}
		else if (objectName == "NPC") {
			Npc& npc = npcs[packet->object_id];
			npc.objectId = packet->object_id;
			npc.x = packet->x;
			npc.y = packet->y;
		}
		else if (packet->object_id >= NPC_ID_START) {
			Monster& monster = monsters[packet->object_id];
			monster.objectId = packet->object_id;
			monster.x = packet->x;
			monster.y = packet->y;
			monster.spriteId = packet->visual_id;
			monster.hp = packet->hp;
			monster.maxHp = packet->max_hp;
		}
		else if (packet->object_id != playerId_) {
			RemotePlayer& player = remotePlayers[packet->object_id];
			player.x = packet->x;
			player.y = packet->y;
			player.hp = packet->hp;
			player.maxHp = packet->max_hp;
			player.exp = packet->exp;
			player.level = packet->level;
		}
		break;
	}
	case S2C_REMOVE_OBJECT: {
		const S2C_RemoveObject* packet = reinterpret_cast<const S2C_RemoveObject*>(data);
		remotePlayers.erase(packet->object_id);
		monsters.erase(packet->object_id);
		potions.erase(packet->object_id);
		npcs.erase(packet->object_id);
		break;
	}
	case S2C_MOVE_OBJECT: {
		const S2C_MoveObject* packet = reinterpret_cast<const S2C_MoveObject*>(data);
		auto npcEntry = npcs.find(packet->object_id);
		if (npcEntry != npcs.end()) {
			Npc& npc = npcEntry->second;
			if (packet->x < npc.x) {
				npc.direction = 1;
			}
			else if (packet->x > npc.x) {
				npc.direction = 2;
			}
			else if (packet->y < npc.y) {
				npc.direction = 3;
			}
			else if (packet->y > npc.y) {
				npc.direction = 0;
			}
			npc.x = packet->x;
			npc.y = packet->y;
			npc.frame = (npc.frame + 1) % CharacterFrameCount;
		}
		else if (packet->object_id >= NPC_ID_START) {
			Monster& monster = monsters[packet->object_id];
			if (packet->x < monster.x) {
				monster.direction = 1;
			}
			else if (packet->x > monster.x) {
				monster.direction = 2;
			}
			monster.objectId = packet->object_id;
			monster.x = packet->x;
			monster.y = packet->y;
		}
		else if (packet->object_id == playerId_) {
			playerX = packet->x;
			playerY = packet->y;
		}
		else {
			RemotePlayer& player = remotePlayers[packet->object_id];
			if (packet->x < player.x) {
				player.direction = 1;
			}
			else if (packet->x > player.x) {
				player.direction = 2;
			}
			else if (packet->y < player.y) {
				player.direction = 3;
			}
			else if (packet->y > player.y) {
				player.direction = 0;
			}
			player.x = packet->x;
			player.y = packet->y;
			player.frame = (player.frame + 1) % CharacterFrameCount;
		}
		break;
	}
	case S2C_CHAT_MESSAGE: {
		const S2C_ChatMessage* packet = reinterpret_cast<const S2C_ChatMessage*>(data);
		ChatMessage chatMessage{};
		chatMessage.objectId = packet->object_id;
		chatMessage.text = PacketText(packet->message, MAX_CHAT_MSG_LEN);
		chatMessages.push_back(chatMessage);
		if (chatMessages.size() > 8) {
			chatMessages.erase(chatMessages.begin());
		}
		break;
	}
	case S2C_ATTACK_OBJECT: {
		const S2C_AttackObject* packet = reinterpret_cast<const S2C_AttackObject*>(data);
		if (packet->object_id >= NPC_ID_START) {
			Monster& monster = monsters[packet->object_id];
			monster.objectId = packet->object_id;
			monster.attacking = true;
			monster.attackDirection = packet->direction;
			monster.attackElapsedMilliseconds = 0;
			if (packet->direction == 1 || packet->direction == 2) {
				monster.direction = packet->direction;
			}
		}
		break;
	}
	case S2C_STATUS_CHANGE: {
		const S2C_StatusChange* packet = reinterpret_cast<const S2C_StatusChange*>(data);
		if (packet->object_id >= NPC_ID_START) {
			auto monster = monsters.find(packet->object_id);
			if (monster != monsters.end()) {
				monster->second.hp = packet->hp;
				monster->second.maxHp = packet->max_hp;
			}
		}
		else {
			auto player = remotePlayers.find(packet->object_id);
			if (packet->object_id == playerId_) {
				playerHp = packet->hp;
				playerMaxHp = packet->max_hp;
				playerExp = packet->exp;
				playerLevel = packet->level;
				playerCanDash = packet->has_dash;
			}
			else if (player != remotePlayers.end()) {
				player->second.hp = packet->hp;
				player->second.maxHp = packet->max_hp;
				player->second.exp = packet->exp;
				player->second.level = packet->level;
			}
		}
		break;
	}
	case S2C_NPC_DIALOGUE: {
		const S2C_NpcDialogue* packet = reinterpret_cast<const S2C_NpcDialogue*>(data);
		auto npc = npcs.find(packet->object_id);
		if (npc != npcs.end()) {
			npc->second.dialogue = PacketText(packet->message, MAX_CHAT_MSG_LEN);
			npc->second.dialogueVisible = true;
			npc->second.dialogueElapsedMilliseconds = 0;
		}
		break;
	}
	case S2C_SKILL_OBJECT: {
		const S2C_SkillObject* packet = reinterpret_cast<const S2C_SkillObject*>(data);
		SkillEffect effect{};
		effect.objectId = packet->object_id;
		effect.visualId = packet->visual_id;
		effect.x = packet->x;
		effect.y = packet->y;
		effect.direction = packet->direction;
		effect.elapsedMilliseconds = 0;
		skillEffects.push_back(effect);
		break;
	}
	case S2C_BOSS_SKILL_OBJECT: {
		const S2C_SkillObject* packet = reinterpret_cast<const S2C_SkillObject*>(data);
		SkillEffect effect{};
		effect.objectId = packet->object_id;
		effect.visualId = packet->visual_id;
		effect.x = packet->x;
		effect.y = packet->y;
		effect.direction = packet->direction;
		effect.elapsedMilliseconds = 0;
		effect.bossSkill = true;
		skillEffects.push_back(effect);
		break;
	}
	default:
		break;
	}
}

void NetworkClient::Disconnect(const char* reason)
{
	std::cout << reason << ". Running local-only mode.\n";
	connected_ = false;
	if (socket_ != INVALID_SOCKET) {
		closesocket(socket_);
		socket_ = INVALID_SOCKET;
	}
}