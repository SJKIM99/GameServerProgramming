#pragma once
#include "global.h"

class sector
{
public:
	std::array<std::array<std::unordered_set<int>, W_WIDTH / SECTOR_RANGE>, W_HEIGHT / SECTOR_RANGE> sectors;
public:
	std::mutex sector_lock;
public:
	sector() {};
	int GetMySector_X(short x);
	int GetMySector_Y(short y);
	
	void AddPlayerInSector(int player_id, short sector_x, short sector_y);
	void RemovePlayerInSector(int player_id, short sector_x, short sector_y);
	bool UpdatePlayerInSector(int player_id, short new_sector_x, short new_sector_y, short old_sector_x, short old_sector_y);
};

