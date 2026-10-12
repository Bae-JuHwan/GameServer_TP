#pragma once

#include <string>

struct RemotePlayer {
	int x = 0;
	int y = 0;
	int hp = 100;
	int maxHp = 100;
	unsigned long long exp = 0;
	unsigned char level = 1;
	int direction = 0;
	int frame = 0;
};

struct Monster {
	int objectId = -1;
	int x = 0;
	int y = 0;
	int spriteId = 1;
	int direction = 2;
	int hp = 10;
	int maxHp = 10;
	bool attacking = false;
	int attackDirection = 0;
	int attackElapsedMilliseconds = 0;
};

struct Potion {
	int objectId = -1;
	int x = 0;
	int y = 0;
	int visualId = 1;
};

struct Npc {
	int objectId = -1;
	int x = 0;
	int y = 0;
	int direction = 0;
	int frame = 0;
	bool dialogueVisible = false;
	int dialogueElapsedMilliseconds = 0;
	std::string dialogue;
};

struct ChatMessage {
	int objectId = -1;
	std::string text;
};

struct SkillEffect {
	int objectId = -1;
	int visualId = 0;
	int x = 0;
	int y = 0;
	int direction = 0;
	int elapsedMilliseconds = 0;
	bool bossSkill = false;
};