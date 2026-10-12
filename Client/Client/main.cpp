#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "asset_paths.h"
#include "asset_loader.h"
#include "audio_manager.h"
#include "client_config.h"
#include "client_types.h"
#include "combat_helpers.h"
#include "input_helpers.h"
#include "map.h"
#include "monster_spawner.h"
#include "network_client.h"
#include "render_helpers.h"
#include "text_helpers.h"
#include "world_utils.h"
#include "zone_helpers.h"

namespace {
	constexpr int ChatVisibleMilliseconds = 5000;
	constexpr int MonsterAttackMilliseconds = 300;
	constexpr int ZoneNoticeVisibleMilliseconds = 3000;
	constexpr int NpcDialogueVisibleMilliseconds = 5000;
	constexpr int SkillEffectMilliseconds = 350;
	constexpr int SkillEffectRangeTiles = 10;
	constexpr int SkillEffectHalfWidth = 1;
	constexpr int DashDistanceTiles = 4;
	constexpr int DashCooldownMilliseconds = 280;
	constexpr int BossSpriteId = 16;
	constexpr int SecondBossSpriteId = 3;
	constexpr int ThirdBossSpriteId = 14;
	constexpr int FourthBossSpriteId = 9;
	constexpr int BossSkillAttack16VisualId = 16;
	constexpr int BossSkillAttack6VisualId = 6;
	constexpr int BossSkillAttack14VisualId = 14;
	constexpr int BossSkillAttack9VisualId = 9;
	constexpr float BossMonsterScaleMultiplier = 5.0f;
	constexpr int BossSkillEffectRangeTiles = 8;
	constexpr int BossCollisionHalfWidthTiles = 4;
	constexpr int BossCollisionTopTiles = 7;

	bool IsBossSprite(int spriteId)
	{
		return spriteId == BossSpriteId || spriteId == SecondBossSpriteId || spriteId == ThirdBossSpriteId || spriteId == FourthBossSpriteId;
	}

	bool IsMonsterAt(const std::unordered_map<int, Monster>& monsters, int x, int y)
	{
		for (const auto& monsterEntry : monsters) {
			const Monster& monster = monsterEntry.second;
			if (IsBossSprite(monster.spriteId)) {
				if (std::abs(monster.x - x) <= BossCollisionHalfWidthTiles &&
					y >= monster.y - BossCollisionTopTiles && y <= monster.y) {
					return true;
				}
				continue;
			}

			if (monster.x == x && monster.y == y) {
				return true;
			}
		}

		return false;
	}

	bool IsNpcAt(const std::unordered_map<int, Npc>& npcs, int x, int y)
	{
		for (const auto& npcEntry : npcs) {
			const Npc& npc = npcEntry.second;
			if (npc.x == x && npc.y == y) {
				return true;
			}
		}

		return false;
	}

	bool IsRemotePlayerAt(const std::unordered_map<int, RemotePlayer>& remotePlayers, int x, int y)
	{
		for (const auto& playerEntry : remotePlayers) {
			const RemotePlayer& player = playerEntry.second;
			if (player.x == x && player.y == y && player.hp > 0) {
				return true;
			}
		}

		return false;
	}

	int FindNearbyNpcId(const std::unordered_map<int, Npc>& npcs, int playerX, int playerY)
	{
		int nearbyNpcId = -1;
		int bestDistance = 5;
		for (const auto& npcEntry : npcs) {
			const Npc& npc = npcEntry.second;
			const int dx = std::abs(npc.x - playerX);
			const int dy = std::abs(npc.y - playerY);
			if (dx > 1 || dy > 1) {
				continue;
			}

			const int distance = dx + dy;
			if (distance < bestDistance) {
				bestDistance = distance;
				nearbyNpcId = npc.objectId;
			}
		}

		return nearbyNpcId;
	}

	sf::String WrappedDialogue(const std::string& text)
	{
		const sf::String source = FromUtf8(text);
		sf::String wrapped;
		int lineLength = 0;
		for (std::size_t i = 0; i < source.getSize(); ++i) {
			wrapped += source[i];
			++lineLength;
			if (lineLength >= 18 && i + 1 < source.getSize()) {
				wrapped += '\n';
				lineLength = 0;
			}
		}

		return wrapped;
	}

	struct MonsterTextureSet {
		sf::Texture right;
		sf::Texture left;
		bool hasRight = false;
		bool hasLeft = false;
	};

	sf::Vector2f AttackOffset(int direction, float amount)
	{
		switch (direction) {
		case 1:
			return sf::Vector2f(-amount, 0.0f);
		case 2:
			return sf::Vector2f(amount, 0.0f);
		case 3:
			return sf::Vector2f(0.0f, -amount);
		default:
			return sf::Vector2f(0.0f, amount);
		}
	}

	float ProjectileRotation(int direction)
	{
		switch (direction) {
		case 1:
			return 180.0f;
		case 2:
			return 0.0f;
		case 3:
			return 270.0f;
		default:
			return 90.0f;
		}
	}

	unsigned long long RequiredExpForNextLevel(unsigned char level)
	{
		return static_cast<unsigned long long>(level) * 100;
	}
}

int main()
{
	const std::string serverAddress = PromptIpAddress("Server IP[127.0.0.1]");
	const std::string playerName = PromptRequiredInput("Player ID");

	sf::RenderWindow window(
		sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT),
		"MMORPG Client - Map Prototype",
		sf::Style::Titlebar | sf::Style::Close);
	window.setFramerateLimit(60);

	const bool usingMciBackgroundMusic = PlayMp3WithMci(AssetCandidates("sound/FloralLife.mp3"), "background music");
	bool backgroundMusicPaused = false;

	Map map;
	map.Generate();

	sf::Texture tileset;
	const bool hasTileset = LoadTextureFromCandidates(tileset, {
		"../../Asset/tileset/tileset.png",
		"../../../Asset/tileset/tileset.png",
		"Asset/tileset/tileset.png",
		"../Asset/tileset/tileset.png"
	}, "tileset");

	if (!hasTileset) {
		std::cout << "Could not load tileset. Falling back to colored tiles.\n";
	}

	sf::Texture characterTexture;
	const bool hasCharacter = LoadTextureFromCandidates(characterTexture, {
		"../../Asset/character overworld/ow10.png",
		"../../../Asset/character overworld/ow10.png",
		"Asset/character overworld/ow10.png",
		"../Asset/character overworld/ow10.png"
	}, "character");

	sf::Texture npcTexture;
	const bool hasNpcTexture = LoadTextureFromCandidates(npcTexture, AssetCandidates("character overworld/ow6.png"), "town NPC");

	std::vector<MonsterTypeDefinition> monsterTypes;
	LoadMonsterTypes(monsterTypes, {
		"../../Asset/monster_spawns.lua",
		"../../../Asset/monster_spawns.lua",
		"Asset/monster_spawns.lua",
		"../Asset/monster_spawns.lua"
	});
	if (monsterTypes.empty()) {
		monsterTypes.push_back(MonsterTypeDefinition{
			1,
			"menusprite1",
			"menu sprites/menusprite1.png",
			"menu sprites/menusprite1-1.png"
		});
	}

	std::unordered_map<int, MonsterTextureSet> monsterTextures;
	for (const MonsterTypeDefinition& monsterType : monsterTypes) {
		MonsterTextureSet& textureSet = monsterTextures[monsterType.spriteId];
		textureSet.hasRight = LoadTextureFromCandidates(
			textureSet.right,
			AssetCandidates(monsterType.rightTexturePath),
			monsterType.name.c_str());
		textureSet.hasLeft = LoadTextureFromCandidates(
			textureSet.left,
			AssetCandidates(monsterType.leftTexturePath),
			monsterType.name.c_str());
	}

	sf::Texture playerAttackTexture;
	const bool hasPlayerAttackTexture = LoadTextureFromCandidates(playerAttackTexture, {
		"../../Asset/player attack effects/player_attack_slash.png",
		"../../../Asset/player attack effects/player_attack_slash.png",
		"Asset/player attack effects/player_attack_slash.png",
		"../Asset/player attack effects/player_attack_slash.png"
	}, "player attack effect");

	sf::Texture skillTexture;
	const bool hasSkillTexture = LoadTextureFromCandidates(skillTexture, AssetCandidates("player skill effects/beginner_skill_projectile_64.png"), "beginner skill projectile");

	sf::Texture bossSkillTexture;
	const bool hasBossSkillTexture = LoadTextureFromCandidates(bossSkillTexture, AssetCandidates("attack effects/boss_attack16_green.png"), "boss attack16 skill");
	sf::Texture bossAttack6Texture;
	const bool hasBossAttack6Texture = LoadTextureFromCandidates(bossAttack6Texture, AssetCandidates("attack effects/boss_attack3_green.png"), "boss attack3 skill");
	sf::Texture bossAttack14Texture;
	const bool hasBossAttack14Texture = LoadTextureFromCandidates(bossAttack14Texture, AssetCandidates("attack effects/boss_attack14_blue.png"), "boss attack14 skill");
	sf::Texture bossAttack9Texture;
	const bool hasBossAttack9Texture = LoadTextureFromCandidates(bossAttack9Texture, AssetCandidates("attack effects/boss_attack9_orange.png"), "boss attack9 skill");

	sf::Texture potionTexture;
	const bool hasPotionTexture = LoadTextureFromCandidates(potionTexture, AssetCandidates("UI/baricon2.png"), "potion");

	sf::Sprite tileSprite;
	sf::Sprite purpleHouseSprite;
	if (hasTileset) {
		tileSprite.setTexture(tileset);
		const float tileScale = static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE);
		tileSprite.setScale(tileScale, tileScale);
		purpleHouseSprite.setTexture(tileset);
		purpleHouseSprite.setTextureRect(PurpleHouseRect());
		purpleHouseSprite.setScale(tileScale, tileScale);
	}

	const std::array<sf::Vector2i, 4> townHouses = {
		sf::Vector2i(760, 760),
		sf::Vector2i(1140, 760),
		sf::Vector2i(760, 1100),
		sf::Vector2i(1140, 1100)
	};

	sf::RectangleShape fallbackTile(sf::Vector2f(static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE)));
	sf::RectangleShape fallbackPlayer(sf::Vector2f(20.0f, 20.0f));
	fallbackPlayer.setFillColor(sf::Color(230, 70, 80));
	fallbackPlayer.setOutlineColor(sf::Color::Black);
	fallbackPlayer.setOutlineThickness(2.0f);
	sf::RectangleShape remoteFallbackPlayer(sf::Vector2f(20.0f, 20.0f));
	remoteFallbackPlayer.setFillColor(sf::Color(80, 120, 230));
	remoteFallbackPlayer.setOutlineColor(sf::Color::Black);
	remoteFallbackPlayer.setOutlineThickness(2.0f);
	sf::RectangleShape playerHpBarBackground;
	playerHpBarBackground.setFillColor(sf::Color(30, 30, 30, 210));
	playerHpBarBackground.setOutlineColor(sf::Color::Black);
	playerHpBarBackground.setOutlineThickness(1.0f);
	sf::RectangleShape playerHpBarFill;
	playerHpBarFill.setFillColor(sf::Color(220, 30, 30));
	sf::RectangleShape bossHpBarBackground;
	bossHpBarBackground.setFillColor(sf::Color(30, 30, 30, 220));
	bossHpBarBackground.setOutlineColor(sf::Color::Black);
	bossHpBarBackground.setOutlineThickness(1.0f);
	sf::RectangleShape bossHpBarFill;
	bossHpBarFill.setFillColor(sf::Color(220, 30, 30));

	sf::Sprite playerSprite;
	if (hasCharacter) {
		playerSprite.setTexture(characterTexture);
		playerSprite.setTextureRect(CharacterRect(0, 0));
		playerSprite.setScale(CharacterScale, CharacterScale);
	}
	sf::Sprite npcSprite;
	if (hasNpcTexture) {
		npcSprite.setTexture(npcTexture);
		npcSprite.setTextureRect(CharacterRect(0, 0));
		npcSprite.setScale(CharacterScale, CharacterScale);
	}

	sf::Sprite monsterSprite;
	sf::Sprite potionSprite;
	if (hasPotionTexture) {
		potionSprite.setTexture(potionTexture);
		potionSprite.setColor(sf::Color(230, 40, 40));
	}

	sf::Sprite playerAttackSprite;
	if (hasPlayerAttackTexture) {
		playerAttackSprite.setTexture(playerAttackTexture);
	}
	sf::Sprite skillSprite;
	if (hasSkillTexture) {
		skillSprite.setTexture(skillTexture);
		skillSprite.setOrigin(
			static_cast<float>(skillTexture.getSize().x) / 2.0f,
			static_cast<float>(skillTexture.getSize().y) / 2.0f);
	}
	sf::Sprite bossSkillSprite;
	if (hasBossSkillTexture) {
		bossSkillSprite.setTexture(bossSkillTexture);
		bossSkillSprite.setOrigin(
			static_cast<float>(bossSkillTexture.getSize().x) / 2.0f,
			static_cast<float>(bossSkillTexture.getSize().y) / 2.0f);
	}
	sf::Sprite bossAttack6Sprite;
	if (hasBossAttack6Texture) {
		bossAttack6Sprite.setTexture(bossAttack6Texture);
		bossAttack6Sprite.setOrigin(
			static_cast<float>(bossAttack6Texture.getSize().x) / 2.0f,
			static_cast<float>(bossAttack6Texture.getSize().y) / 2.0f);
	}
	sf::Sprite bossAttack14Sprite;
	if (hasBossAttack14Texture) {
		bossAttack14Sprite.setTexture(bossAttack14Texture);
		bossAttack14Sprite.setOrigin(
			static_cast<float>(bossAttack14Texture.getSize().x) / 2.0f,
			static_cast<float>(bossAttack14Texture.getSize().y) / 2.0f);
	}
	sf::Sprite bossAttack9Sprite;
	if (hasBossAttack9Texture) {
		bossAttack9Sprite.setTexture(bossAttack9Texture);
		bossAttack9Sprite.setOrigin(
			static_cast<float>(bossAttack9Texture.getSize().x) / 2.0f,
			static_cast<float>(bossAttack9Texture.getSize().y) / 2.0f);
	}

	std::unordered_map<int, Monster> monsters;
	std::unordered_map<int, Potion> potions;
	std::unordered_map<int, Npc> npcs;

	sf::Font uiFont;
	const bool hasUiFont = LoadFontFromCandidates(uiFont, {
		"C:/Windows/Fonts/malgun.ttf",
		"C:/Windows/Fonts/arial.ttf",
	}, "UI font");

	sf::Text coordinateText;
	sf::Text zoneNoticeText;
	sf::Text chatLogText;
	sf::Text chatInputText;
	sf::Text npcDialogueText;
	sf::RectangleShape coordinateBackground;
	sf::RectangleShape zoneNoticeBackground;
	sf::RectangleShape chatBackground;
	sf::RectangleShape chatInputBackground;
	sf::RectangleShape npcDialogueBackground;
	sf::ConvexShape npcDialogueTail(3);
	if (hasUiFont) {
		coordinateText.setFont(uiFont);
		coordinateText.setCharacterSize(16);
		coordinateText.setFillColor(sf::Color::White);
		coordinateText.setOutlineColor(sf::Color::Black);
		coordinateText.setOutlineThickness(2.0f);
		coordinateBackground.setFillColor(sf::Color(0, 0, 0, 130));

		zoneNoticeText.setFont(uiFont);
		zoneNoticeText.setCharacterSize(24);
		zoneNoticeText.setFillColor(sf::Color::White);
		zoneNoticeText.setOutlineColor(sf::Color::Black);
		zoneNoticeText.setOutlineThickness(2.0f);
		zoneNoticeBackground.setFillColor(sf::Color(0, 0, 0, 150));

		chatLogText.setFont(uiFont);
		chatLogText.setCharacterSize(16);
		chatLogText.setFillColor(sf::Color::White);
		chatLogText.setOutlineColor(sf::Color::Black);
		chatLogText.setOutlineThickness(2.0f);
		chatBackground.setFillColor(sf::Color(0, 0, 0, 120));

		chatInputText.setFont(uiFont);
		chatInputText.setCharacterSize(17);
		chatInputText.setFillColor(sf::Color::White);
		chatInputText.setOutlineColor(sf::Color::Black);
		chatInputText.setOutlineThickness(2.0f);
		chatInputBackground.setFillColor(sf::Color(20, 20, 20, 190));

		npcDialogueText.setFont(uiFont);
		npcDialogueText.setCharacterSize(15);
		npcDialogueText.setFillColor(sf::Color::Black);
		npcDialogueBackground.setFillColor(sf::Color::White);
		npcDialogueBackground.setOutlineColor(sf::Color::Black);
		npcDialogueBackground.setOutlineThickness(2.0f);
		npcDialogueTail.setFillColor(sf::Color::White);
		npcDialogueTail.setOutlineColor(sf::Color::Black);
		npcDialogueTail.setOutlineThickness(1.0f);
	}

	const sf::Vector2i startPosition = RandomOpenPosition(map);
	int playerX = startPosition.x;
	int playerY = startPosition.y;
	int playerHp = 100;
	int playerMaxHp = 100;
	unsigned long long playerExp = 0;
	unsigned char playerLevel = 1;
	int playerDirection = 0;
	int playerFrame = 0;
	bool playerAttacking = false;
	int playerAttackDirection = 0;
	int playerAttackFrame = 0;
	bool playerCanDash = false;
	bool freeCamera = false;
	int debugCameraX = ClampInt(playerX - SCREEN_TILE_X / 2, 0, WORLD_WIDTH - SCREEN_TILE_X);
	int debugCameraY = ClampInt(playerY - SCREEN_TILE_Y / 2, 0, WORLD_HEIGHT - SCREEN_TILE_Y);
	int debugVisibleTiles = SCREEN_TILE_X;
	bool windowFocused = true;
	std::unordered_map<int, RemotePlayer> remotePlayers;
	std::vector<ChatMessage> chatMessages;
	std::vector<SkillEffect> skillEffects;
	bool chatActive = false;
	sf::String chatInput;
	NetworkClient network;
	network.Connect(serverAddress.c_str(), playerName.c_str());
	sf::Clock moveClock;
	sf::Clock animationClock;
	sf::Clock chatDisplayClock;
	sf::Clock zoneNoticeClock;
	sf::Clock playerAttackClock;
	sf::Clock dashClock;
	sf::Clock frameClock;
	bool showZoneNotice = false;
	sf::String zoneNoticeMessage;

	const auto tryMoveBySteps = [&](int stepX, int stepY, int stepCount, int moveTime) {
		int nextX = playerX;
		int nextY = playerY;
		for (int step = 0; step < stepCount; ++step) {
			const int candidateX = ClampInt(nextX + stepX, 0, WORLD_WIDTH - 1);
			const int candidateY = ClampInt(nextY + stepY, 0, WORLD_HEIGHT - 1);
			if ((candidateX == nextX && candidateY == nextY) ||
				map.IsBlocked(candidateX, candidateY) ||
				IsMonsterAt(monsters, candidateX, candidateY) ||
				IsNpcAt(npcs, candidateX, candidateY) ||
				IsRemotePlayerAt(remotePlayers, candidateX, candidateY)) {
				break;
			}
			nextX = candidateX;
			nextY = candidateY;
		}

		if (nextX == playerX && nextY == playerY) {
			return false;
		}

		playerX = nextX;
		playerY = nextY;
		network.SendMove(
			static_cast<short>(playerX),
			static_cast<short>(playerY),
			moveTime);
		return true;
	};

	while (window.isOpen()) {
		const int frameMilliseconds = frameClock.restart().asMilliseconds();
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				window.close();
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
				if (chatActive) {
					chatActive = false;
					chatInput.clear();
				}
				else {
					window.close();
				}
			}
			else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter) {
				if (chatActive) {
					const std::string message = ToUtf8(chatInput);
					if (!message.empty()) {
						network.SendChat(message);
						chatDisplayClock.restart();
					}
					chatInput.clear();
					chatActive = false;
				}
				else {
					chatActive = true;
				}
			}
			else if (event.type == sf::Event::KeyPressed && chatActive && event.key.code == sf::Keyboard::BackSpace) {
				if (!chatInput.isEmpty()) {
					chatInput.erase(chatInput.getSize() - 1);
				}
			}
			else if (event.type == sf::Event::TextEntered && chatActive) {
				if (event.text.unicode >= 32 && event.text.unicode != 127 && CanAppendChatChar(chatInput, event.text.unicode)) {
					chatInput += event.text.unicode;
				}
			}
			else if (event.type == sf::Event::KeyPressed &&
				!chatActive &&
				((event.key.code >= sf::Keyboard::Num0 && event.key.code <= sf::Keyboard::Num8) ||
					(event.key.code >= sf::Keyboard::Numpad0 && event.key.code <= sf::Keyboard::Numpad8))) {
				int sectionX = 0;
				int sectionY = 0;
				if (SectionFromNumberKey(event.key.code, sectionX, sectionY)) {
					const bool townTeleport = event.key.code == sf::Keyboard::Num0 || event.key.code == sf::Keyboard::Numpad0;
					sf::Vector2i teleportPosition;
					if (townTeleport) {
						static std::random_device randomDevice;
						static std::mt19937 generator(randomDevice());
						std::uniform_int_distribution<std::size_t> houseDistribution(0, townHouses.size() - 1);
						const sf::Vector2i housePosition = townHouses[houseDistribution(generator)];
						teleportPosition = sf::Vector2i(
							housePosition.x + PurpleHouseTileWidth / 2,
							housePosition.y + PurpleHouseTileHeight);
					}
					else {
						teleportPosition = RandomOpenPositionInSection(map, sectionX, sectionY);
					}
					playerX = teleportPosition.x;
					playerY = teleportPosition.y;
					playerFrame = 0;
					if (event.key.code == sf::Keyboard::Num1 || event.key.code == sf::Keyboard::Numpad1 ||
						event.key.code == sf::Keyboard::Num3 || event.key.code == sf::Keyboard::Numpad3) {
						zoneNoticeMessage = BeginnerZoneNotice();
						showZoneNotice = true;
						zoneNoticeClock.restart();
					}
					else if (event.key.code == sf::Keyboard::Num2 || event.key.code == sf::Keyboard::Numpad2) {
						zoneNoticeMessage = IntermediateZoneNotice();
						showZoneNotice = true;
						zoneNoticeClock.restart();
					}
					else if (event.key.code == sf::Keyboard::Num4 || event.key.code == sf::Keyboard::Numpad4) {
						zoneNoticeMessage = AdvancedZoneNotice();
						showZoneNotice = true;
						zoneNoticeClock.restart();
					}
					if (freeCamera) {
						debugCameraX = ClampInt(playerX - debugVisibleTiles / 2, 0, WORLD_WIDTH - debugVisibleTiles);
						debugCameraY = ClampInt(playerY - debugVisibleTiles / 2, 0, WORLD_HEIGHT - debugVisibleTiles);
					}
					network.SendMove(
						static_cast<short>(playerX),
						static_cast<short>(playerY),
						0);
				}
			}
			else if (event.type == sf::Event::KeyPressed && !chatActive && event.key.code == sf::Keyboard::C) {
				freeCamera = !freeCamera;
				if (freeCamera) {
					debugCameraX = ClampInt(playerX - debugVisibleTiles / 2, 0, WORLD_WIDTH - debugVisibleTiles);
					debugCameraY = ClampInt(playerY - debugVisibleTiles / 2, 0, WORLD_HEIGHT - debugVisibleTiles);
				}
			}
			else if (event.type == sf::Event::KeyPressed && !chatActive && event.key.code == sf::Keyboard::A) {
				if (!playerAttacking) {
					playerAttacking = true;
					playerAttackDirection = playerDirection;
					playerAttackFrame = 0;
					playerAttackClock.restart();
					network.SendAttack(playerDirection);
				}
			}
			else if (event.type == sf::Event::KeyPressed && !chatActive && event.key.code == sf::Keyboard::F) {
				const int npcId = FindNearbyNpcId(npcs, playerX, playerY);
				if (npcId != -1) {
					network.SendInteract(npcId);
				}
			}
			else if (event.type == sf::Event::KeyPressed && !chatActive && event.key.code == sf::Keyboard::S) {
				network.SendSkill(playerDirection);
			}
			else if (event.type == sf::Event::KeyPressed &&
				!chatActive &&
				!freeCamera &&
				playerCanDash &&
				(event.key.code == sf::Keyboard::LShift || event.key.code == sf::Keyboard::RShift) &&
				dashClock.getElapsedTime().asMilliseconds() >= DashCooldownMilliseconds) {
				const sf::Vector2i dashOffset = DirectionOffset(playerDirection);
				if (tryMoveBySteps(dashOffset.x, dashOffset.y, DashDistanceTiles, 0)) {
					dashClock.restart();
					playerFrame = (playerFrame + 1) % CharacterFrameCount;
				}
			}
			else if (event.type == sf::Event::KeyPressed && !chatActive && freeCamera && event.key.code == sf::Keyboard::Q) {
				debugVisibleTiles = std::min(72, debugVisibleTiles + 12);
				debugCameraX = ClampInt(debugCameraX, 0, WORLD_WIDTH - debugVisibleTiles);
				debugCameraY = ClampInt(debugCameraY, 0, WORLD_HEIGHT - debugVisibleTiles);
			}
			else if (event.type == sf::Event::KeyPressed && !chatActive && freeCamera && event.key.code == sf::Keyboard::E) {
				debugVisibleTiles = std::max(SCREEN_TILE_X, debugVisibleTiles - 12);
				debugCameraX = ClampInt(debugCameraX, 0, WORLD_WIDTH - debugVisibleTiles);
				debugCameraY = ClampInt(debugCameraY, 0, WORLD_HEIGHT - debugVisibleTiles);
			}
			else if (event.type == sf::Event::LostFocus) {
				windowFocused = false;
			}
			else if (event.type == sf::Event::GainedFocus) {
				windowFocused = true;
			}
		}

		const std::size_t previousChatMessageCount = chatMessages.size();
		network.ProcessPackets(playerX, playerY, playerHp, playerMaxHp, playerExp, playerLevel, playerCanDash, remotePlayers, monsters, potions, npcs, chatMessages, skillEffects);
		if (chatMessages.size() != previousChatMessageCount) {
			chatDisplayClock.restart();
		}

		if (moveClock.getElapsedTime().asMilliseconds() >= 120) {
			int stepX = 0;
			int stepY = 0;
			bool moving = false;

			if (freeCamera && windowFocused && !chatActive) {
				const int cameraStep = 8;
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
					debugCameraX -= cameraStep;
				}
				else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
					debugCameraX += cameraStep;
				}
				if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
					debugCameraY -= cameraStep;
				}
				else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
					debugCameraY += cameraStep;
				}

				debugCameraX = ClampInt(debugCameraX, 0, WORLD_WIDTH - debugVisibleTiles);
				debugCameraY = ClampInt(debugCameraY, 0, WORLD_HEIGHT - debugVisibleTiles);
			}
			else if (windowFocused && !chatActive && sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
				stepX = -1;
				playerDirection = 1;
				moving = true;
			}
			else if (windowFocused && !chatActive && sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
				stepX = 1;
				playerDirection = 2;
				moving = true;
			}
			else if (windowFocused && !chatActive && sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
				stepY = -1;
				playerDirection = 3;
				moving = true;
			}
			else if (windowFocused && !chatActive && sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
				stepY = 1;
				playerDirection = 0;
				moving = true;
			}

			if (moving) {
				tryMoveBySteps(stepX, stepY, 1, moveClock.getElapsedTime().asMilliseconds());
			}

			if (moving && animationClock.getElapsedTime().asMilliseconds() >= 120) {
				playerFrame = (playerFrame + 1) % CharacterFrameCount;
				animationClock.restart();
			}
			else if (!moving) {
				playerFrame = 0;
			}

			moveClock.restart();
		}

		SetBackgroundMusicPaused(
			usingMciBackgroundMusic,
			IsTownSectorPosition(playerX, playerY),
			backgroundMusicPaused);

		if (playerAttacking &&
			playerAttackClock.getElapsedTime().asMilliseconds() >= PlayerAttackFrameMilliseconds) {
			++playerAttackFrame;
			if (playerAttackFrame >= PlayerAttackFrameCount) {
				playerAttacking = false;
				playerAttackFrame = 0;
			}
			playerAttackClock.restart();
		}

		for (auto& monsterEntry : monsters) {
			Monster& monster = monsterEntry.second;
			if (!monster.attacking) {
				continue;
			}

			monster.attackElapsedMilliseconds += frameMilliseconds;
			if (monster.attackElapsedMilliseconds >= MonsterAttackMilliseconds) {
				monster.attacking = false;
				monster.attackElapsedMilliseconds = 0;
			}
		}

		for (auto& npcEntry : npcs) {
			Npc& npc = npcEntry.second;
			if (!npc.dialogueVisible) {
				continue;
			}

			npc.dialogueElapsedMilliseconds += frameMilliseconds;
			if (npc.dialogueElapsedMilliseconds >= NpcDialogueVisibleMilliseconds) {
				npc.dialogueVisible = false;
				npc.dialogueElapsedMilliseconds = 0;
			}
		}

		for (SkillEffect& effect : skillEffects) {
			effect.elapsedMilliseconds += frameMilliseconds;
		}
		skillEffects.erase(
			std::remove_if(skillEffects.begin(), skillEffects.end(), [](const SkillEffect& effect) {
				return effect.elapsedMilliseconds >= SkillEffectMilliseconds;
			}),
			skillEffects.end());

		const int visibleTileX = freeCamera ? debugVisibleTiles : SCREEN_TILE_X;
		const int visibleTileY = freeCamera ? debugVisibleTiles : SCREEN_TILE_Y;
		const float drawTileSize = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(visibleTileX);
		const float renderScale = drawTileSize / static_cast<float>(TILE_SIZE);
		const int cameraX = freeCamera ? debugCameraX : ClampInt(playerX - SCREEN_TILE_X / 2, 0, WORLD_WIDTH - SCREEN_TILE_X);
		const int cameraY = freeCamera ? debugCameraY : ClampInt(playerY - SCREEN_TILE_Y / 2, 0, WORLD_HEIGHT - SCREEN_TILE_Y);

		window.clear(sf::Color::Black);

		for (int screenY = 0; screenY < visibleTileY; ++screenY) {
			for (int screenX = 0; screenX < visibleTileX; ++screenX) {
				const int worldX = cameraX + screenX;
				const int worldY = cameraY + screenY;
				const TileType tile = map.GetTile(worldX, worldY);
				const float drawX = static_cast<float>(screenX) * drawTileSize;
				const float drawY = static_cast<float>(screenY) * drawTileSize;

				if (hasTileset) {
					tileSprite.setTextureRect(TileRect(tile));
					tileSprite.setScale(renderScale * static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE),
						renderScale * static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE));
					tileSprite.setPosition(drawX, drawY);
					window.draw(tileSprite);
				}
				else {
					switch (tile) {
					case TILE_SAND:
						fallbackTile.setFillColor(sf::Color(210, 190, 120));
						break;
					case TILE_WATER:
						fallbackTile.setFillColor(sf::Color(60, 120, 210));
						break;
					case TILE_TREE:
						fallbackTile.setFillColor(sf::Color(40, 120, 60));
						break;
					case TILE_ROCK:
						fallbackTile.setFillColor(sf::Color(110, 110, 110));
						break;
					case TILE_HOUSE:
						fallbackTile.setFillColor(sf::Color(150, 95, 55));
						break;
					default:
						fallbackTile.setFillColor(sf::Color(80, 170, 90));
						break;
					}
					fallbackTile.setSize(sf::Vector2f(drawTileSize, drawTileSize));
					fallbackTile.setPosition(drawX, drawY);
					window.draw(fallbackTile);
				}
			}
		}
		if (hasTileset) {
			const float tileScale = static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE);
			tileSprite.setScale(tileScale, tileScale);
		}

		if (freeCamera) {
			sf::RectangleShape sectionLine;
			sectionLine.setFillColor(sf::Color(255, 255, 255, 160));

			for (int sectionX = 1; sectionX < MAP_SECTION_COLS; ++sectionX) {
				const int worldBoundaryX = MapSectionMinX(sectionX);
				if (worldBoundaryX >= cameraX && worldBoundaryX < cameraX + visibleTileX) {
					const float screenX = static_cast<float>(worldBoundaryX - cameraX) * drawTileSize;
					sectionLine.setSize(sf::Vector2f(2.0f, static_cast<float>(SCREEN_HEIGHT)));
					sectionLine.setPosition(screenX, 0.0f);
					window.draw(sectionLine);
				}
			}

			for (int sectionY = 1; sectionY < MAP_SECTION_ROWS; ++sectionY) {
				const int worldBoundaryY = MapSectionMinY(sectionY);
				if (worldBoundaryY >= cameraY && worldBoundaryY < cameraY + visibleTileY) {
					const float screenY = static_cast<float>(worldBoundaryY - cameraY) * drawTileSize;
					sectionLine.setSize(sf::Vector2f(static_cast<float>(SCREEN_WIDTH), 2.0f));
					sectionLine.setPosition(0.0f, screenY);
					window.draw(sectionLine);
				}
			}
		}

		if (hasTileset) {
			for (const sf::Vector2i& housePosition : townHouses) {
				if (housePosition.x + PurpleHouseTileWidth <= cameraX ||
					housePosition.y + PurpleHouseTileHeight <= cameraY ||
					housePosition.x >= cameraX + visibleTileX ||
					housePosition.y >= cameraY + visibleTileY) {
					continue;
				}

				purpleHouseSprite.setScale(renderScale * static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE),
					renderScale * static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE));
				purpleHouseSprite.setPosition(
					static_cast<float>(housePosition.x - cameraX) * drawTileSize,
					static_cast<float>(housePosition.y - cameraY) * drawTileSize);
				window.draw(purpleHouseSprite);
			}
			const float tileScale = static_cast<float>(TILE_SIZE) / static_cast<float>(TILE_SOURCE_SIZE);
			purpleHouseSprite.setScale(tileScale, tileScale);
		}

		const float playerScreenX = static_cast<float>(playerX - cameraX) * drawTileSize;
		const float playerScreenY = static_cast<float>(playerY - cameraY) * drawTileSize;
		const float characterDrawSize = static_cast<float>(CharacterFrameSize) * CharacterScale * renderScale;

		for (const auto& monsterEntry : monsters) {
			const Monster& monster = monsterEntry.second;
			if (monster.x < cameraX || monster.y < cameraY ||
				monster.x >= cameraX + visibleTileX || monster.y >= cameraY + visibleTileY) {
				continue;
			}

			auto textureEntry = monsterTextures.find(monster.spriteId);
			if (textureEntry == monsterTextures.end()) {
				textureEntry = monsterTextures.find(1);
			}
			if (textureEntry == monsterTextures.end() || (!textureEntry->second.hasRight && !textureEntry->second.hasLeft)) {
				continue;
			}

			const MonsterTextureSet& textureSet = textureEntry->second;
			const sf::Texture& currentMonsterTexture =
				(monster.direction == 1 && textureSet.hasLeft) || !textureSet.hasRight
				? textureSet.left
				: textureSet.right;
			const float monsterScale = MonsterScale * (IsBossSprite(monster.spriteId) ? BossMonsterScaleMultiplier : 1.0f);
			const float monsterDrawWidth = static_cast<float>(currentMonsterTexture.getSize().x) * monsterScale * renderScale;
			const float monsterDrawHeight = static_cast<float>(currentMonsterTexture.getSize().y) * monsterScale * renderScale;

			monsterSprite.setTexture(currentMonsterTexture, true);
			monsterSprite.setScale(renderScale * monsterScale, renderScale * monsterScale);
			sf::Vector2f attackDrawOffset;
			if (monster.attacking) {
				const float progress = static_cast<float>(monster.attackElapsedMilliseconds) /
					static_cast<float>(MonsterAttackMilliseconds);
				const float lunge = (progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f) * 0.5f * drawTileSize;
				attackDrawOffset = AttackOffset(monster.attackDirection, lunge);
			}

			const float monsterDrawX =
				static_cast<float>(monster.x - cameraX) * drawTileSize + (drawTileSize - monsterDrawWidth) / 2.0f + attackDrawOffset.x;
			const float monsterDrawY =
				static_cast<float>(monster.y - cameraY) * drawTileSize + drawTileSize - monsterDrawHeight + attackDrawOffset.y;
			monsterSprite.setPosition(monsterDrawX, monsterDrawY);
			window.draw(monsterSprite);

			if (IsBossSprite(monster.spriteId) && monster.maxHp > 0) {
				const float bossHpRatio = std::max(
					0.0f,
					std::min(1.0f, static_cast<float>(monster.hp) / static_cast<float>(monster.maxHp)));
				const float bossHpBarWidth = std::max(80.0f * renderScale, monsterDrawWidth * 0.75f);
				const float bossHpBarHeight = std::max(6.0f * renderScale, 8.0f);
				const float bossHpBarX = monsterDrawX + (monsterDrawWidth - bossHpBarWidth) / 2.0f;
				const float bossHpBarY = monsterDrawY - bossHpBarHeight - 6.0f * renderScale;

				bossHpBarBackground.setSize(sf::Vector2f(bossHpBarWidth, bossHpBarHeight));
				bossHpBarBackground.setPosition(bossHpBarX, bossHpBarY);
				bossHpBarFill.setSize(sf::Vector2f(bossHpBarWidth * bossHpRatio, bossHpBarHeight));
				bossHpBarFill.setPosition(bossHpBarX, bossHpBarY);
				window.draw(bossHpBarBackground);
				window.draw(bossHpBarFill);
			}
		}
		monsterSprite.setScale(1.0f, 1.0f);

		if (hasPotionTexture) {
			for (const auto& potionEntry : potions) {
				const Potion& potion = potionEntry.second;
				if (potion.x < cameraX || potion.y < cameraY ||
					potion.x >= cameraX + visibleTileX || potion.y >= cameraY + visibleTileY) {
					continue;
				}

				const float potionSizeMultiplier = potion.visualId == 2 ? 4.0f : 1.0f;
				const float potionDrawSize = std::min(drawTileSize * 0.7f, 28.0f * renderScale) * potionSizeMultiplier;
				const float potionScaleX = potionDrawSize / static_cast<float>(potionTexture.getSize().x);
				const float potionScaleY = potionDrawSize / static_cast<float>(potionTexture.getSize().y);
				potionSprite.setScale(potionScaleX, potionScaleY);
				potionSprite.setPosition(
					static_cast<float>(potion.x - cameraX) * drawTileSize + (drawTileSize - potionDrawSize) / 2.0f,
					static_cast<float>(potion.y - cameraY) * drawTileSize + (drawTileSize - potionDrawSize) / 2.0f);
				window.draw(potionSprite);
			}
			potionSprite.setScale(1.0f, 1.0f);
		}

		if (hasNpcTexture) {
			for (const auto& npcEntry : npcs) {
				const Npc& npc = npcEntry.second;
				if (npc.x < cameraX || npc.y < cameraY ||
					npc.x >= cameraX + visibleTileX || npc.y >= cameraY + visibleTileY) {
					continue;
				}

				const float npcScreenX = static_cast<float>(npc.x - cameraX) * drawTileSize;
				const float npcScreenY = static_cast<float>(npc.y - cameraY) * drawTileSize;
				npcSprite.setTextureRect(CharacterRect(npc.direction, npc.frame));
				npcSprite.setScale(CharacterScale * renderScale, CharacterScale * renderScale);
				npcSprite.setPosition(
					npcScreenX + (drawTileSize - characterDrawSize) / 2.0f,
					npcScreenY + drawTileSize - characterDrawSize);
				window.draw(npcSprite);

				if (hasUiFont && npc.dialogueVisible && !npc.dialogue.empty()) {
					npcDialogueText.setString(WrappedDialogue(npc.dialogue));
					const sf::FloatRect textBounds = npcDialogueText.getLocalBounds();
					const float paddingX = 10.0f;
					const float paddingY = 7.0f;
					const float bubbleWidth = textBounds.width + paddingX * 2.0f;
					const float bubbleHeight = textBounds.height + paddingY * 2.0f;
					float bubbleX = npcScreenX + drawTileSize * 0.8f;
					float bubbleY = npcScreenY - bubbleHeight - 8.0f;
					bubbleX = std::min(bubbleX, static_cast<float>(SCREEN_WIDTH) - bubbleWidth - 6.0f);
					bubbleY = std::max(6.0f, bubbleY);

					npcDialogueBackground.setSize(sf::Vector2f(bubbleWidth, bubbleHeight));
					npcDialogueBackground.setPosition(bubbleX, bubbleY);
					npcDialogueText.setPosition(
						bubbleX + paddingX - textBounds.left,
						bubbleY + paddingY - textBounds.top);

					const float tailX = std::max(bubbleX + 8.0f, npcScreenX + drawTileSize * 0.75f);
					const float tailY = bubbleY + bubbleHeight - 1.0f;
					npcDialogueTail.setPoint(0, sf::Vector2f(tailX, tailY));
					npcDialogueTail.setPoint(1, sf::Vector2f(tailX + 12.0f, tailY));
					npcDialogueTail.setPoint(2, sf::Vector2f(npcScreenX + drawTileSize * 0.65f, npcScreenY + drawTileSize * 0.1f));

					window.draw(npcDialogueTail);
					window.draw(npcDialogueBackground);
					window.draw(npcDialogueText);
				}
			}
			npcSprite.setScale(CharacterScale, CharacterScale);
		}

		if (hasSkillTexture || hasBossSkillTexture || hasBossAttack6Texture || hasBossAttack14Texture || hasBossAttack9Texture) {
			for (const SkillEffect& effect : skillEffects) {
				const bool useBossAttack16 = effect.bossSkill &&
					(effect.visualId == BossSkillAttack16VisualId || effect.visualId == 0);
				const bool useBossAttack6 = effect.bossSkill && effect.visualId == BossSkillAttack6VisualId;
				const bool useBossAttack14 = effect.bossSkill && effect.visualId == BossSkillAttack14VisualId;
				const bool useBossAttack9 = effect.bossSkill && effect.visualId == BossSkillAttack9VisualId;
				if (useBossAttack16 && !hasBossSkillTexture) {
					continue;
				}
				if (useBossAttack6 && !hasBossAttack6Texture) {
					continue;
				}
				if (useBossAttack14 && !hasBossAttack14Texture) {
					continue;
				}
				if (useBossAttack9 && !hasBossAttack9Texture) {
					continue;
				}
				if (effect.bossSkill && !useBossAttack16 && !useBossAttack6 && !useBossAttack14 && !useBossAttack9) {
					continue;
				}
				if (!effect.bossSkill && !hasSkillTexture) {
					continue;
				}

				sf::Sprite& effectSprite = useBossAttack9 ? bossAttack9Sprite : (useBossAttack14 ? bossAttack14Sprite : (useBossAttack6 ? bossAttack6Sprite : (effect.bossSkill ? bossSkillSprite : skillSprite)));
				const sf::Texture& effectTexture = useBossAttack9 ? bossAttack9Texture : (useBossAttack14 ? bossAttack14Texture : (useBossAttack6 ? bossAttack6Texture : (effect.bossSkill ? bossSkillTexture : skillTexture)));
				const sf::Vector2i directionOffset = DirectionOffset(effect.direction);
				const float progress = std::min(
					1.0f,
					static_cast<float>(effect.elapsedMilliseconds) / static_cast<float>(SkillEffectMilliseconds));
				const int effectRangeTiles = effect.bossSkill ? BossSkillEffectRangeTiles : SkillEffectRangeTiles;
				const float worldX = static_cast<float>(effect.x) +
					static_cast<float>(directionOffset.x) * static_cast<float>(effectRangeTiles) * progress;
				const float worldY = static_cast<float>(effect.y) +
					static_cast<float>(directionOffset.y) * static_cast<float>(effectRangeTiles) * progress;
				if (worldX < static_cast<float>(cameraX - 1) || worldY < static_cast<float>(cameraY - 1) ||
					worldX > static_cast<float>(cameraX + visibleTileX + 1) ||
					worldY > static_cast<float>(cameraY + visibleTileY + 1)) {
					continue;
				}

				const float screenX = (worldX - static_cast<float>(cameraX)) * drawTileSize + drawTileSize / 2.0f;
				const float screenY = (worldY - static_cast<float>(cameraY)) * drawTileSize + drawTileSize / 2.0f;
				const float projectileLength = effect.bossSkill
					? std::max(drawTileSize * 2.8f, 96.0f * renderScale)
					: std::max(drawTileSize * 2.2f, 66.0f * renderScale);
				const float projectileWidth = drawTileSize * static_cast<float>(SkillEffectHalfWidth * 2 + 1) *
					(effect.bossSkill ? 2.4f : 1.8f);
				const float scaleX = projectileLength / static_cast<float>(effectTexture.getSize().x);
				const float scaleY = projectileWidth / static_cast<float>(effectTexture.getSize().y);
				effectSprite.setScale(scaleX, scaleY);
				effectSprite.setRotation(ProjectileRotation(effect.direction));
				effectSprite.setPosition(screenX, screenY);
				window.draw(effectSprite);
			}
			skillSprite.setScale(1.0f, 1.0f);
			skillSprite.setRotation(0.0f);
			bossSkillSprite.setScale(1.0f, 1.0f);
			bossSkillSprite.setRotation(0.0f);
			bossAttack6Sprite.setScale(1.0f, 1.0f);
			bossAttack6Sprite.setRotation(0.0f);
			bossAttack14Sprite.setScale(1.0f, 1.0f);
			bossAttack14Sprite.setRotation(0.0f);
			bossAttack9Sprite.setScale(1.0f, 1.0f);
			bossAttack9Sprite.setRotation(0.0f);
		}

		for (const auto& remotePlayerEntry : remotePlayers) {
			const RemotePlayer& remotePlayer = remotePlayerEntry.second;
			if (remotePlayer.x < cameraX || remotePlayer.y < cameraY ||
				remotePlayer.x >= cameraX + visibleTileX || remotePlayer.y >= cameraY + visibleTileY) {
				continue;
			}

			const float remoteScreenX = static_cast<float>(remotePlayer.x - cameraX) * drawTileSize;
			const float remoteScreenY = static_cast<float>(remotePlayer.y - cameraY) * drawTileSize;

			if (hasCharacter) {
				playerSprite.setTextureRect(CharacterRect(remotePlayer.direction, remotePlayer.frame));
				playerSprite.setScale(CharacterScale * renderScale, CharacterScale * renderScale);
				playerSprite.setPosition(
					remoteScreenX + (drawTileSize - characterDrawSize) / 2.0f,
					remoteScreenY + drawTileSize - characterDrawSize);
				window.draw(playerSprite);
			}
			else {
				remoteFallbackPlayer.setPosition(remoteScreenX + 2.0f, remoteScreenY + 2.0f);
				window.draw(remoteFallbackPlayer);
			}
		}

		if (playerAttacking && hasPlayerAttackTexture) {
			const float attackDrawSize = static_cast<float>(PlayerAttackFrameSize) * PlayerAttackScale * renderScale;

			const int attackDirectionCount = playerLevel >= 20 ? 8 : 4;
			for (int attackDirection = 0; attackDirection < attackDirectionCount; ++attackDirection) {
				if (playerLevel < 10 && attackDirection != playerAttackDirection) {
					continue;
				}

				const sf::Vector2i attackOffset = DirectionOffset(attackDirection);
				const int attackX = playerX + attackOffset.x;
				const int attackY = playerY + attackOffset.y;
				const float attackScreenX = static_cast<float>(attackX - cameraX) * drawTileSize;
				const float attackScreenY = static_cast<float>(attackY - cameraY) * drawTileSize;

				if (attackX >= cameraX && attackY >= cameraY &&
					attackX < cameraX + visibleTileX && attackY < cameraY + visibleTileY) {
					playerAttackSprite.setTextureRect(PlayerAttackRect(AttackEffectDirection(attackDirection), playerAttackFrame));
					playerAttackSprite.setScale(PlayerAttackScale * renderScale, PlayerAttackScale * renderScale);
					playerAttackSprite.setOrigin(
						static_cast<float>(PlayerAttackFrameSize) / 2.0f,
						static_cast<float>(PlayerAttackFrameSize) / 2.0f);
					playerAttackSprite.setRotation(AttackEffectRotation(attackDirection));
					playerAttackSprite.setPosition(
						attackScreenX + drawTileSize / 2.0f,
						attackScreenY + drawTileSize / 2.0f);
					window.draw(playerAttackSprite);
				}
			}
			playerAttackSprite.setScale(1.0f, 1.0f);
			playerAttackSprite.setOrigin(0.0f, 0.0f);
			playerAttackSprite.setRotation(0.0f);
		}

		if (hasCharacter) {
			playerSprite.setTextureRect(CharacterRect(playerDirection, playerFrame));
			playerSprite.setScale(CharacterScale * renderScale, CharacterScale * renderScale);
			playerSprite.setPosition(
				playerScreenX + (drawTileSize - characterDrawSize) / 2.0f,
				playerScreenY + drawTileSize - characterDrawSize);
			window.draw(playerSprite);
			playerSprite.setScale(CharacterScale, CharacterScale);
		}
		else {
			fallbackPlayer.setPosition(playerScreenX + 2.0f, playerScreenY + 2.0f);
			window.draw(fallbackPlayer);
		}

		const float hpBarWidth = std::max(24.0f, drawTileSize * 0.9f);
		const float hpBarHeight = std::max(4.0f, 5.0f * renderScale);
		const float hpBarX = playerScreenX + (drawTileSize - hpBarWidth) / 2.0f + 1;
		const float hpBarY = playerScreenY - hpBarHeight - 27.0f;
		const float hpRatio = playerMaxHp > 0
			? static_cast<float>(std::max(0, playerHp)) / static_cast<float>(playerMaxHp)
			: 0.0f;
		playerHpBarBackground.setSize(sf::Vector2f(hpBarWidth, hpBarHeight));
		playerHpBarBackground.setPosition(hpBarX, hpBarY);
		playerHpBarFill.setSize(sf::Vector2f(hpBarWidth * std::min(1.0f, hpRatio), hpBarHeight));
		playerHpBarFill.setPosition(hpBarX, hpBarY);
		window.draw(playerHpBarBackground);
		window.draw(playerHpBarFill);

		if (hasUiFont) {
			const std::string playerStatusText =
				"  |  LV: " + std::to_string(static_cast<int>(playerLevel)) +
				"  EXP: " + std::to_string(playerExp) + "/" + std::to_string(RequiredExpForNextLevel(playerLevel));

			if (freeCamera) {
				coordinateText.setString(
					"CAM X: " + std::to_string(cameraX) + "  Y: " + std::to_string(cameraY) +
					"  Zoom: " + std::to_string(debugVisibleTiles) + " tiles" +
					"  |  Player X: " + std::to_string(playerX) + "  Y: " + std::to_string(playerY) +
					playerStatusText);
			}
			else {
				coordinateText.setString(
					"X: " + std::to_string(playerX) + "  Y: " + std::to_string(playerY) +
					playerStatusText);
			}

			const sf::FloatRect textBounds = coordinateText.getLocalBounds();
			const float padding = 8.0f;
			coordinateBackground.setSize(sf::Vector2f(
				textBounds.width + padding * 2.0f,
				textBounds.height + padding * 2.0f));
			coordinateBackground.setPosition(
				static_cast<float>(SCREEN_WIDTH) - coordinateBackground.getSize().x - padding,
				padding);
			coordinateText.setPosition(
				coordinateBackground.getPosition().x + (coordinateBackground.getSize().x - textBounds.width) / 2.0f - textBounds.left,
				coordinateBackground.getPosition().y + (coordinateBackground.getSize().y - textBounds.height) / 2.0f - textBounds.top);

			window.draw(coordinateBackground);
			window.draw(coordinateText);

			if (showZoneNotice) {
				if (zoneNoticeClock.getElapsedTime().asMilliseconds() >= ZoneNoticeVisibleMilliseconds) {
					showZoneNotice = false;
				}
				else {
					zoneNoticeText.setString(zoneNoticeMessage);
					const sf::FloatRect noticeBounds = zoneNoticeText.getLocalBounds();
					const float noticePaddingX = 18.0f;
					const float noticePaddingY = 10.0f;
					zoneNoticeBackground.setSize(sf::Vector2f(
						noticeBounds.width + noticePaddingX * 2.0f,
						noticeBounds.height + noticePaddingY * 2.0f));
					zoneNoticeBackground.setPosition(
						(static_cast<float>(SCREEN_WIDTH) - zoneNoticeBackground.getSize().x) / 2.0f,
						18.0f);
					zoneNoticeText.setPosition(
						zoneNoticeBackground.getPosition().x + (zoneNoticeBackground.getSize().x - noticeBounds.width) / 2.0f - noticeBounds.left,
						zoneNoticeBackground.getPosition().y + (zoneNoticeBackground.getSize().y - noticeBounds.height) / 2.0f - noticeBounds.top);

					window.draw(zoneNoticeBackground);
					window.draw(zoneNoticeText);
				}
			}

			const bool showChatPanel = chatActive ||
				(!chatMessages.empty() && chatDisplayClock.getElapsedTime().asMilliseconds() < ChatVisibleMilliseconds);

			if (showChatPanel) {
				const float chatPanelWidth = 360.0f;
				const float chatPanelHeight = 180.0f;
				const float chatPanelX = 10.0f;
				const float chatPanelY = static_cast<float>(SCREEN_HEIGHT) - chatPanelHeight - 10.0f;
				const float chatInputHeight = 36.0f;

				chatBackground.setSize(sf::Vector2f(chatPanelWidth, chatPanelHeight));
				chatBackground.setPosition(chatPanelX, chatPanelY);
				window.draw(chatBackground);

				sf::String chatLog;
				for (const ChatMessage& chatMessage : chatMessages) {
					chatLog += FromUtf8(chatMessage.text);
					chatLog += "\n";
				}

				chatLogText.setString(chatLog);
				const sf::FloatRect chatBounds = chatLogText.getLocalBounds();
				chatLogText.setPosition(chatPanelX + 10.0f - chatBounds.left, chatPanelY + 10.0f - chatBounds.top);
				window.draw(chatLogText);

				if (chatActive) {
					chatInputBackground.setSize(sf::Vector2f(chatPanelWidth - 12.0f, chatInputHeight));
					chatInputBackground.setPosition(chatPanelX + 6.0f, chatPanelY + chatPanelHeight - chatInputHeight - 6.0f);
					chatInputText.setString(sf::String("> ") + chatInput);
					window.draw(chatInputBackground);
					chatInputText.setPosition(chatInputBackground.getPosition().x + 8.0f, chatInputBackground.getPosition().y + 6.0f);
					window.draw(chatInputText);
				}
			}
		}

		window.display();
	}

	if (usingMciBackgroundMusic) {
		CloseBackgroundMusic();
	}

	return 0;
}