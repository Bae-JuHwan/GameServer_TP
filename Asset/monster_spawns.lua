-- map numbers:
-- 5 1 6
-- 2 0 3
-- 7 4 8

local monster_types = {
   [1] = {
	  name = "menusprite1",
	  right_image = "menu sprites/menusprite1-1.png",
	  left_image = "menu sprites/menusprite1.png",
	  hp = 10,
	  attack = 10,
	  exp = 100
   },
   [5] = {
	  name = "menusprite5",
	  right_image = "menu sprites/menusprite5-1.png",
	  left_image = "menu sprites/menusprite5.png",
	  hp = 10,
	  attack = 10,
	  exp = 100
   },
   [6] = {
	  name = "menusprite6",
	  right_image = "menu sprites/menusprite6-1.png",
	  left_image = "menu sprites/menusprite6.png",
	  hp = 40,
	  attack = 20,
	  exp = 200
   },
   [11] = {
	  name = "menusprite11",
	  right_image = "menu sprites/menusprite11-1.png",
	  left_image = "menu sprites/menusprite11.png",
	  hp = 80,
	  attack = 40,
	  exp = 400
   },
   [16] = {
	  name = "menusprite16",
	  right_image = "menu sprites/menusprite16-1.png",
	  left_image = "menu sprites/menusprite16.png",
	  hp = 1000,
	  attack = 50,
	  exp = 2000
   },
   [3] = {
	  name = "menusprite3",
	  right_image = "menu sprites/menusprite3-1.png",
	  left_image = "menu sprites/menusprite3.png",
	  hp = 1000,
	  attack = 50,
	  exp = 2000
   },
   [14] = {
	  name = "menusprite14",
	  right_image = "menu sprites/menusprite14-1.png",
	  left_image = "menu sprites/menusprite14.png",
	  hp = 1000,
	  attack = 50,
	  exp = 2000
   },
   [9] = {
	  name = "menusprite9",
	  right_image = "menu sprites/menusprite9-1.png",
	  left_image = "menu sprites/menusprite9.png",
	  hp = 1000,
	  attack = 50,
	  exp = 2000
   }
}

local section_spawns = {
   [1] = {
	  monster_type_id = 1,
	  count = 20000
   },
   [2] = {
	  monster_type_id = 6,
	  count = 20000
   },
   [3] = {
	  monster_type_id = 5,
	  count = 20000
   },
   [4] = {
	  monster_type_id = 11,
	  count = 20000
   },
   [5] = {
	  monster_type_id = 16,
	  count = 500
   },
   [6] = {
	  monster_type_id = 3,
	  count = 500
   },
   [7] = {
	  monster_type_id = 14,
	  count = 500
   },
   [8] = {
	  monster_type_id = 9,
	  count = 500
   }
}

function load_monster_types()
   for monster_type_id, monster in pairs(monster_types) do
	  monster_type(monster_type_id, monster.name, monster.right_image, monster.left_image, monster.hp, monster.attack, monster.exp)
   end
end

function load_spawn_sections()
   for map_number, spawn in pairs(section_spawns) do
	  section(map_number, spawn.monster_type_id, spawn.count)
   end
end

load_monster_types()
load_spawn_sections()