#include <iostream>
#include <thread>

#include "database.h"
#include "monster_ai.h"
#include "monster_spawner.h"
#include "npc_spawner.h"
#include "potion_spawner.h"
#include "protocol_2026.h"
#include "server_network.h"
#include "server_platform.h"

#pragma comment(lib, "ws2_32.lib")

int main()
{
	SetConsoleOutputCP(CP_UTF8);

	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed\n";
		return 1;
	}

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET) {
		std::cerr << "socket failed\n";
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddress{};
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(PORT);

	if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
		std::cerr << "bind failed. port=" << PORT << '\n';
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "listen failed\n";
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "Server listening on port " << PORT << '\n';
	if (!g_db.connect()) {}
	LoadMonsterSpawnScript({
		"../../Asset/monster_spawns.lua",
		"../../../Asset/monster_spawns.lua",
		"Asset/monster_spawns.lua",
		"../Asset/monster_spawns.lua"
	});
	SpawnPotions();
	SpawnTownNpcs();
	std::thread(MonsterAiLoop).detach();

	int nextPlayerId = 1;
	while (true) {
		SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET) {
			std::cerr << "accept failed\n";
			continue;
		}

		std::thread(HandleClient, clientSocket, nextPlayerId++).detach();
	}

	closesocket(listenSocket);
	WSACleanup();
	return 0;
}