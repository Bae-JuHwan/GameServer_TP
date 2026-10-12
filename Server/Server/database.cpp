#include "database.h"

#include <Windows.h>
#include <sqlext.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <mutex>

#pragma comment(lib, "odbc32.lib")

namespace {
	SQLHENV g_env = SQL_NULL_HENV;
	SQLHDBC g_conn = SQL_NULL_HDBC;
	std::mutex g_dbMutex;

	const char* DefaultConnectionString()
	{
		return "DSN=2026_ODBC;";
	}

	const char* ConnectionString()
	{
		static char value[1024]{};
		const DWORD length = GetEnvironmentVariableA("GAME_DB_CONN", value, sizeof(value));
		if (length > 0 && length < sizeof(value)) {
			return value;
		}

		return DefaultConnectionString();
	}

	bool Succeeded(SQLRETURN ret)
	{
		return ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO;
	}

	void PrintOdbcError(SQLSMALLINT handleType, SQLHANDLE handle, const char* where)
	{
		bool printed = false;
		for (SQLSMALLINT record = 1; ; ++record) {
			SQLCHAR state[6]{};
			SQLCHAR message[SQL_MAX_MESSAGE_LENGTH]{};
			SQLINTEGER nativeError = 0;
			SQLSMALLINT textLength = 0;
			const SQLRETURN ret = SQLGetDiagRecA(handleType, handle, record, state, &nativeError,
				message, sizeof(message), &textLength);
			if (ret == SQL_NO_DATA) {
				break;
			}
			if (SQL_SUCCEEDED(ret)) {
				printed = true;
			}
		}

		if (!printed) {}
	}

	bool ExecDirect(const char* sql)
	{
		SQLHSTMT stmt = SQL_NULL_HSTMT;
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, g_conn, &stmt);
		if (!Succeeded(ret)) {
			return false;
		}

		ret = SQLExecDirectA(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)), SQL_NTS);
		if (!Succeeded(ret)) {
			PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLExecDirect");
			SQLFreeHandle(SQL_HANDLE_STMT, stmt);
			return false;
		}

		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return true;
	}

	void PrintCurrentDatabase()
	{
		SQLHSTMT stmt = SQL_NULL_HSTMT;
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, g_conn, &stmt);
		if (!Succeeded(ret)) {
			return;
		}

		const char* sql = "SELECT @@SERVERNAME, DB_NAME()";
		ret = SQLExecDirectA(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)), SQL_NTS);
		if (!Succeeded(ret)) {
			PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLExecDirect PrintCurrentDatabase");
			SQLFreeHandle(SQL_HANDLE_STMT, stmt);
			return;
		}

		SQLCHAR serverName[128]{};
		SQLCHAR databaseName[128]{};
		SQLLEN serverNameLen = 0;
		SQLLEN databaseNameLen = 0;
		SQLBindCol(stmt, 1, SQL_C_CHAR, serverName, sizeof(serverName), &serverNameLen);
		SQLBindCol(stmt, 2, SQL_C_CHAR, databaseName, sizeof(databaseName), &databaseNameLen);

		ret = SQLFetch(stmt);
		if (Succeeded(ret)) {}

		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	}

	bool EnsureSchema()
	{
		if (!ExecDirect(
			"IF OBJECT_ID('dbo.PlayerState', 'U') IS NULL "
			"CREATE TABLE dbo.PlayerState ("
			"player_name VARCHAR(20) NOT NULL PRIMARY KEY,"
			"pos_x SMALLINT NOT NULL,"
			"pos_y SMALLINT NOT NULL,"
			"player_hp INT NOT NULL,"
			"player_max_hp INT NOT NULL,"
			"player_attack_damage INT NOT NULL,"
			"player_exp BIGINT NOT NULL,"
			"player_level INT NOT NULL,"
			"beginner_quest_accepted BIT NOT NULL DEFAULT 0,"
			"beginner_quest_kills INT NOT NULL DEFAULT 0,"
			"beginner_quest_complete BIT NOT NULL DEFAULT 0,"
			"has_beginner_skill BIT NOT NULL DEFAULT 0,"
			"intermediate_quest_accepted BIT NOT NULL DEFAULT 0,"
			"intermediate_quest_kills INT NOT NULL DEFAULT 0,"
			"intermediate_quest_complete BIT NOT NULL DEFAULT 0,"
			"has_dash BIT NOT NULL DEFAULT 0,"
			"updated_at DATETIME2 NOT NULL DEFAULT SYSDATETIME()"
			")")) {
			return false;
		}

		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'beginner_quest_accepted') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD beginner_quest_accepted BIT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'beginner_quest_kills') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD beginner_quest_kills INT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'beginner_quest_complete') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD beginner_quest_complete BIT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'has_beginner_skill') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD has_beginner_skill BIT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'intermediate_quest_accepted') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD intermediate_quest_accepted BIT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'intermediate_quest_kills') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD intermediate_quest_kills INT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'intermediate_quest_complete') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD intermediate_quest_complete BIT NOT NULL DEFAULT 0");
		ExecDirect("IF COL_LENGTH('dbo.PlayerState', 'has_dash') IS NULL "
			"ALTER TABLE dbo.PlayerState ADD has_dash BIT NOT NULL DEFAULT 0");

		SQLHSTMT stmt = SQL_NULL_HSTMT;
		SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, g_conn, &stmt);
		if (!Succeeded(ret)) {
			return true;
		}

		const char* sql =
			"SELECT OBJECT_ID('dbo.PlayerState', 'U')";
		ret = SQLExecDirectA(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)), SQL_NTS);
		if (Succeeded(ret)) {
			SQLINTEGER objectId = 0;
			SQLLEN objectIdLen = 0;
			SQLBindCol(stmt, 1, SQL_C_SLONG, &objectId, 0, &objectIdLen);
			if (Succeeded(SQLFetch(stmt)) && objectIdLen != SQL_NULL_DATA) {}
			else {}
		}
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return true;
	}

	void ApplyLevelStats(ClientState& player)
	{
		if (player.level >= PlayerSecondPowerUpLevel) {
			player.maxHp = PlayerSecondPowerUpHp;
			player.attackDamage = PlayerSecondPowerUpAttackDamage;
		}
		else if (player.level >= PlayerPowerUpLevel) {
			player.maxHp = PlayerPowerUpHp;
			player.attackDamage = PlayerPowerUpAttackDamage;
		}
		else {
			player.maxHp = PlayerInitialHp;
			player.attackDamage = PlayerInitialAttackDamage;
		}

		player.hp = std::max(1, std::min(player.hp, player.maxHp));
	}

	bool BindPlayerSaveParameters(SQLHSTMT stmt, const ClientState& player, SQLSMALLINT nameParameterIndex,
		SQLSMALLINT firstValueParameterIndex, SQLSMALLINT& x, SQLSMALLINT& y, SQLINTEGER& hp,
		SQLINTEGER& maxHp, SQLINTEGER& attackDamage, SQLBIGINT& exp, SQLINTEGER& level,
		SQLCHAR& questAccepted, SQLINTEGER& questKills, SQLCHAR& questComplete, SQLCHAR& hasBeginnerSkill,
		SQLCHAR& intermediateQuestAccepted, SQLINTEGER& intermediateQuestKills,
		SQLCHAR& intermediateQuestComplete, SQLCHAR& hasDash, SQLLEN indicators[15], SQLLEN& playerNameLen)
	{
		x = static_cast<SQLSMALLINT>(player.x);
		y = static_cast<SQLSMALLINT>(player.y);
		hp = player.hp;
		maxHp = player.maxHp;
		attackDamage = player.attackDamage;
		exp = static_cast<SQLBIGINT>(std::min<unsigned long long>(
			player.exp, static_cast<unsigned long long>(std::numeric_limits<SQLBIGINT>::max())));
		level = static_cast<SQLINTEGER>(player.level);
		questAccepted = player.beginnerQuestAccepted ? 1 : 0;
		questKills = static_cast<SQLINTEGER>(player.beginnerQuestKills);
		questComplete = player.beginnerQuestComplete ? 1 : 0;
		hasBeginnerSkill = player.hasBeginnerSkill ? 1 : 0;
		intermediateQuestAccepted = player.intermediateQuestAccepted ? 1 : 0;
		intermediateQuestKills = static_cast<SQLINTEGER>(player.intermediateQuestKills);
		intermediateQuestComplete = player.intermediateQuestComplete ? 1 : 0;
		hasDash = player.hasDash ? 1 : 0;
		std::fill(indicators, indicators + 15, 0);
		playerNameLen = SQL_NTS;

		SQLSMALLINT index = firstValueParameterIndex;
		SQLRETURN ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_SMALLINT,
			0, 0, &x, 0, &indicators[0]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_SMALLINT,
			0, 0, &y, 0, &indicators[1]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
			0, 0, &hp, 0, &indicators[2]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
			0, 0, &maxHp, 0, &indicators[3]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
			0, 0, &attackDamage, 0, &indicators[4]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
			0, 0, &exp, 0, &indicators[5]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
			0, 0, &level, 0, &indicators[6]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT,
			0, 0, &questAccepted, 0, &indicators[7]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
			0, 0, &questKills, 0, &indicators[8]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT,
			0, 0, &questComplete, 0, &indicators[9]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT,
			0, 0, &hasBeginnerSkill, 0, &indicators[10]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT,
			0, 0, &intermediateQuestAccepted, 0, &indicators[11]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
			0, 0, &intermediateQuestKills, 0, &indicators[12]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT,
			0, 0, &intermediateQuestComplete, 0, &indicators[13]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, index++, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT,
			0, 0, &hasDash, 0, &indicators[14]);
		if (!Succeeded(ret)) {
			return false;
		}
		ret = SQLBindParameter(stmt, nameParameterIndex, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
			20, 0, const_cast<char*>(player.name.c_str()), 0, &playerNameLen);
		return Succeeded(ret);
	}
}

PlayerDatabase g_db;

bool PlayerDatabase::connect()
{
	std::lock_guard<std::mutex> lock(g_dbMutex);
	if (g_conn != SQL_NULL_HDBC) {
		return true;
	}

	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &g_env);
	if (!Succeeded(ret)) {
		return false;
	}

	ret = SQLSetEnvAttr(g_env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_ENV, g_env, "SQLSetEnvAttr");
		SQLFreeHandle(SQL_HANDLE_ENV, g_env);
		g_env = SQL_NULL_HENV;
		return false;
	}

	ret = SQLAllocHandle(SQL_HANDLE_DBC, g_env, &g_conn);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_ENV, g_env, "SQLAllocHandle DBC");
		SQLFreeHandle(SQL_HANDLE_ENV, g_env);
		g_env = SQL_NULL_HENV;
		return false;
	}

	SQLSetConnectAttr(g_conn, SQL_LOGIN_TIMEOUT, reinterpret_cast<SQLPOINTER>(5), 0);

	SQLCHAR outConn[1024]{};
	SQLSMALLINT outLen = 0;
	ret = SQLDriverConnectA(g_conn, nullptr,
		reinterpret_cast<SQLCHAR*>(const_cast<char*>(ConnectionString())), SQL_NTS,
		outConn, sizeof(outConn), &outLen, SQL_DRIVER_NOPROMPT);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_DBC, g_conn, "SQLDriverConnect");
		SQLFreeHandle(SQL_HANDLE_DBC, g_conn);
		g_conn = SQL_NULL_HDBC;
		SQLFreeHandle(SQL_HANDLE_ENV, g_env);
		g_env = SQL_NULL_HENV;
		return false;
	}

	if (!EnsureSchema()) {
		disconnect();
		return false;
	}

	PrintCurrentDatabase();
	return true;
}

void PlayerDatabase::disconnect()
{
	if (g_conn != SQL_NULL_HDBC) {
		SQLDisconnect(g_conn);
		SQLFreeHandle(SQL_HANDLE_DBC, g_conn);
		g_conn = SQL_NULL_HDBC;
	}
	if (g_env != SQL_NULL_HENV) {
		SQLFreeHandle(SQL_HANDLE_ENV, g_env);
		g_env = SQL_NULL_HENV;
	}
}

bool PlayerDatabase::is_connected() const
{
	return g_conn != SQL_NULL_HDBC;
}

bool PlayerDatabase::load_player(const char* playerName, ClientState& player)
{
	std::lock_guard<std::mutex> lock(g_dbMutex);
	if (g_conn == SQL_NULL_HDBC) {
		return false;
	}

	SQLHSTMT stmt = SQL_NULL_HSTMT;
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, g_conn, &stmt);
	if (!Succeeded(ret)) {
		return false;
	}

	const char* sql =
		"SELECT pos_x, pos_y, player_hp, player_max_hp, player_attack_damage, player_exp, player_level, "
		"beginner_quest_accepted, beginner_quest_kills, beginner_quest_complete, has_beginner_skill, "
		"intermediate_quest_accepted, intermediate_quest_kills, intermediate_quest_complete, has_dash "
		"FROM dbo.PlayerState WHERE player_name = ?";
	ret = SQLPrepareA(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(sql)), SQL_NTS);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLPrepare load_player");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLLEN playerNameLen = SQL_NTS;
	SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
		20, 0, const_cast<char*>(playerName), 0, &playerNameLen);

	ret = SQLExecute(stmt);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLExecute load_player");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLSMALLINT x = 0;
	SQLSMALLINT y = 0;
	SQLINTEGER hp = PlayerInitialHp;
	SQLINTEGER maxHp = PlayerInitialHp;
	SQLINTEGER attackDamage = PlayerInitialAttackDamage;
	SQLBIGINT exp = 0;
	SQLINTEGER level = PlayerInitialLevel;
	SQLCHAR questAccepted = 0;
	SQLINTEGER questKills = 0;
	SQLCHAR questComplete = 0;
	SQLCHAR hasBeginnerSkill = 0;
	SQLCHAR intermediateQuestAccepted = 0;
	SQLINTEGER intermediateQuestKills = 0;
	SQLCHAR intermediateQuestComplete = 0;
	SQLCHAR hasDash = 0;
	SQLLEN indicators[15]{};

	SQLBindCol(stmt, 1, SQL_C_SSHORT, &x, 0, &indicators[0]);
	SQLBindCol(stmt, 2, SQL_C_SSHORT, &y, 0, &indicators[1]);
	SQLBindCol(stmt, 3, SQL_C_SLONG, &hp, 0, &indicators[2]);
	SQLBindCol(stmt, 4, SQL_C_SLONG, &maxHp, 0, &indicators[3]);
	SQLBindCol(stmt, 5, SQL_C_SLONG, &attackDamage, 0, &indicators[4]);
	SQLBindCol(stmt, 6, SQL_C_SBIGINT, &exp, 0, &indicators[5]);
	SQLBindCol(stmt, 7, SQL_C_SLONG, &level, 0, &indicators[6]);
	SQLBindCol(stmt, 8, SQL_C_BIT, &questAccepted, 0, &indicators[7]);
	SQLBindCol(stmt, 9, SQL_C_SLONG, &questKills, 0, &indicators[8]);
	SQLBindCol(stmt, 10, SQL_C_BIT, &questComplete, 0, &indicators[9]);
	SQLBindCol(stmt, 11, SQL_C_BIT, &hasBeginnerSkill, 0, &indicators[10]);
	SQLBindCol(stmt, 12, SQL_C_BIT, &intermediateQuestAccepted, 0, &indicators[11]);
	SQLBindCol(stmt, 13, SQL_C_SLONG, &intermediateQuestKills, 0, &indicators[12]);
	SQLBindCol(stmt, 14, SQL_C_BIT, &intermediateQuestComplete, 0, &indicators[13]);
	SQLBindCol(stmt, 15, SQL_C_BIT, &hasDash, 0, &indicators[14]);

	ret = SQLFetch(stmt);
	if (ret == SQL_NO_DATA) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLFetch load_player");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	player.x = x;
	player.y = y;
	player.hp = hp;
	player.maxHp = maxHp;
	player.attackDamage = attackDamage;
	player.exp = static_cast<unsigned long long>(std::max<SQLBIGINT>(0, exp));
	player.level = static_cast<unsigned char>(std::max<SQLINTEGER>(PlayerInitialLevel, level));
	player.beginnerQuestAccepted = questAccepted != 0;
	player.beginnerQuestKills = std::max<SQLINTEGER>(0, questKills);
	player.beginnerQuestComplete = questComplete != 0;
	player.hasBeginnerSkill = hasBeginnerSkill != 0;
	player.intermediateQuestAccepted = intermediateQuestAccepted != 0;
	player.intermediateQuestKills = std::max<SQLINTEGER>(0, intermediateQuestKills);
	player.intermediateQuestComplete = intermediateQuestComplete != 0;
	player.hasDash = hasDash != 0;
	ApplyLevelStats(player);

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	return true;
}

bool PlayerDatabase::save_player(const ClientState& player)
{
	std::lock_guard<std::mutex> lock(g_dbMutex);
	if (g_conn == SQL_NULL_HDBC || player.name.empty()) {
		return false;
	}

	SQLHSTMT stmt = SQL_NULL_HSTMT;
	SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, g_conn, &stmt);
	if (!Succeeded(ret)) {
		return false;
	}

	const char* saveSql =
		"MERGE dbo.PlayerState AS target "
		"USING (SELECT CAST(? AS VARCHAR(20)) AS player_name, CAST(? AS SMALLINT) AS pos_x, "
		"CAST(? AS SMALLINT) AS pos_y, CAST(? AS INT) AS player_hp, CAST(? AS INT) AS player_max_hp, "
		"CAST(? AS INT) AS player_attack_damage, CAST(? AS BIGINT) AS player_exp, CAST(? AS INT) AS player_level, "
		"CAST(? AS BIT) AS beginner_quest_accepted, CAST(? AS INT) AS beginner_quest_kills, "
		"CAST(? AS BIT) AS beginner_quest_complete, CAST(? AS BIT) AS has_beginner_skill, "
		"CAST(? AS BIT) AS intermediate_quest_accepted, CAST(? AS INT) AS intermediate_quest_kills, "
		"CAST(? AS BIT) AS intermediate_quest_complete, CAST(? AS BIT) AS has_dash) AS source "
		"ON target.player_name = source.player_name "
		"WHEN MATCHED THEN UPDATE SET pos_x = source.pos_x, pos_y = source.pos_y, player_hp = source.player_hp, "
		"player_max_hp = source.player_max_hp, player_attack_damage = source.player_attack_damage, "
		"player_exp = source.player_exp, player_level = source.player_level, "
		"beginner_quest_accepted = source.beginner_quest_accepted, beginner_quest_kills = source.beginner_quest_kills, "
		"beginner_quest_complete = source.beginner_quest_complete, has_beginner_skill = source.has_beginner_skill, "
		"intermediate_quest_accepted = source.intermediate_quest_accepted, intermediate_quest_kills = source.intermediate_quest_kills, "
		"intermediate_quest_complete = source.intermediate_quest_complete, has_dash = source.has_dash, updated_at = SYSDATETIME() "
		"WHEN NOT MATCHED THEN INSERT (player_name, pos_x, pos_y, player_hp, player_max_hp, player_attack_damage, "
		"player_exp, player_level, beginner_quest_accepted, beginner_quest_kills, beginner_quest_complete, has_beginner_skill, "
		"intermediate_quest_accepted, intermediate_quest_kills, intermediate_quest_complete, has_dash) "
		"VALUES (source.player_name, source.pos_x, source.pos_y, source.player_hp, source.player_max_hp, source.player_attack_damage, "
		"source.player_exp, source.player_level, source.beginner_quest_accepted, source.beginner_quest_kills, "
		"source.beginner_quest_complete, source.has_beginner_skill, source.intermediate_quest_accepted, "
		"source.intermediate_quest_kills, source.intermediate_quest_complete, source.has_dash);";
	ret = SQLPrepareA(stmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(saveSql)), SQL_NTS);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLPrepare save_player procedure");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLSMALLINT x = 0;
	SQLSMALLINT y = 0;
	SQLINTEGER hp = 0;
	SQLINTEGER maxHp = 0;
	SQLINTEGER attackDamage = 0;
	SQLBIGINT exp = 0;
	SQLINTEGER level = 0;
	SQLCHAR questAccepted = 0;
	SQLINTEGER questKills = 0;
	SQLCHAR questComplete = 0;
	SQLCHAR hasBeginnerSkill = 0;
	SQLCHAR intermediateQuestAccepted = 0;
	SQLINTEGER intermediateQuestKills = 0;
	SQLCHAR intermediateQuestComplete = 0;
	SQLCHAR hasDash = 0;
	SQLLEN indicators[15]{};
	SQLLEN playerNameLen = SQL_NTS;
	if (!BindPlayerSaveParameters(stmt, player, 1, 2, x, y, hp, maxHp, attackDamage, exp, level,
		questAccepted, questKills, questComplete, hasBeginnerSkill,
		intermediateQuestAccepted, intermediateQuestKills, intermediateQuestComplete, hasDash,
		indicators, playerNameLen)) {
		PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLBindParameter save_player procedure");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	ret = SQLExecute(stmt);
	if (!Succeeded(ret)) {
		PrintOdbcError(SQL_HANDLE_STMT, stmt, "SQLExecute save_player procedure");
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return false;
	}

	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	return true;
}