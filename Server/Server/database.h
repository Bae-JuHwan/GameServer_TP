#pragma once

#include "server_types.h"

class PlayerDatabase {
public:
	bool connect();
	void disconnect();
	bool is_connected() const;

	bool load_player(const char* playerName, ClientState& player);
	bool save_player(const ClientState& player);
};

extern PlayerDatabase g_db;