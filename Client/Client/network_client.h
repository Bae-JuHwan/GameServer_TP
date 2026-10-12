#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <unordered_map>
#include <vector>

#include "client_types.h"
#include "protocol_2026.h"

class NetworkClient {
public:
	~NetworkClient();

	bool Connect(const char* address, const char* username);
	bool IsConnected() const;
	void ProcessPackets(
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
		std::vector<SkillEffect>& skillEffects);
	void SendMove(short x, short y, int moveTime);
	void SendChat(const std::string& message);
	void SendAttack(int direction);
	void SendInteract(int objectId);
	void SendSkill(int direction);

private:
	void SendLogin(const char* username);
	void SendLogout();
	bool SendAll(const void* data, int size);
	void HandlePacket(
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
		std::vector<SkillEffect>& skillEffects);
	void Disconnect(const char* reason);

	WSADATA wsaData_{};
	SOCKET socket_ = INVALID_SOCKET;
	bool wsaStarted_ = false;
	bool connected_ = false;
	int playerId_ = -1;
	std::vector<char> receiveBuffer_;
};