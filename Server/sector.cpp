#include "sector.h"

int sector::GetMySector_X(short x)
{
	short sector_x = x / SECTOR_RANGE;
	return sector_x;
}

int sector::GetMySector_Y(short y)
{
	short sector_y = y / SECTOR_RANGE;
	return sector_y;
}

void sector::AddPlayerInSector(int player_id, short sector_x, short sector_y)
{
	sectors[sector_y][sector_x].insert(player_id);
}

bool sector::UpdatePlayerInSector(int player_id, short new_sector_x, short new_sector_y, short old_sector_x, short old_sector_y) {

    if (new_sector_x != old_sector_x || new_sector_y != old_sector_y) {
        sectors[old_sector_y][old_sector_x].erase(player_id);
        sectors[new_sector_y][new_sector_x].insert(player_id);
        return true;
    }
    return false;
}

void sector::RemovePlayerInSector(int player_id, short sector_x, short sector_y)
{
    sectors[sector_y][sector_x].erase(player_id);
}



