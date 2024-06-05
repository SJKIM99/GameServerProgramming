#include "session.h"
#include "npc.h"
#include "global.h"
#include "collision.h"
#include "sector.h"

bool is_pc(int obj_id)
{
	return obj_id < MAX_USER;
}

bool is_npc(int object_id)
{
	return !is_pc(object_id);
}

void do_npc_random_move(int npc_id)
{
	SESSION& npc = clients[npc_id];
	unordered_set<int> old_viewlist;

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			int sector_y = npc.now_my_sector_y + dy;
			int sector_x = npc.now_my_sector_x + dx;
			// 유효한 섹터 범위인지 확인 
			if (sector_y < 0 || sector_y >= W_WIDTH / SECTOR_RANGE || sector_x < 0 || sector_x >= W_HEIGHT / SECTOR_RANGE) {
				continue;
			}
			const auto& old_sector = sectors.sectors[sector_y][sector_x];
			for (const auto& player_id : old_sector) {
				const auto& player = clients[player_id];
				if (player.state != ST_INGAME) continue;
				if (true == is_npc(player.id)) continue;
				if (!can_see(player.id, npc_id)) continue;
				old_viewlist.insert(player_id);
			}
		}
	}

	int x = npc.x;
	int y = npc.y;

	switch (rand() % 4) {
	case 0:
		if (y > 0) {
			--y;
			if (!is_collision(x, y)) y;
			else ++y;
		}
		break;
	case 1:
		if (y < W_HEIGHT - 1) {
			++y;
			if (!is_collision(x, y)) y;
			else --y;
		}
		break;
	case 2:
		if (x > 0) {
			--x;
			if (!is_collision(x, y)) x;
			else ++x;
		}
		break;
	case 3:
		if (x < W_WIDTH - 1) {
			++x;
			if (!is_collision(x, y)) x;
			else --x;
		}
		break;
	}

	//섹터가 바뀌었나 확인하는 코드
	//npc.sector_lock.lock();
	if (sectors.UpdatePlayerInSector(npc_id, sectors.GetMySector_X(x), sectors.GetMySector_Y(y), sectors.GetMySector_X(npc.x), sectors.GetMySector_Y(npc.y))) {
		clients[npc_id].prev_my_sector_x = sectors.GetMySector_X(npc.x);
		clients[npc_id].prev_my_sector_y = sectors.GetMySector_Y(npc.y);
		clients[npc_id].now_my_sector_x = sectors.GetMySector_X(x);
		clients[npc_id].now_my_sector_y = sectors.GetMySector_Y(y);
	}
	//npc.sector_lock.unlock();

	npc.x = x;
	npc.y = y;

	unordered_set<int> new_vl;

	// 현재 섹터와 주변 8개의 섹터를 순회한다.
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			int sector_y = npc.now_my_sector_y + dy;
			int sector_x = npc.now_my_sector_x + dx;
			// 유효한 섹터 범위인지 확인 
			if (sector_y < 0 || sector_y >= W_WIDTH / SECTOR_RANGE || sector_x < 0 || sector_x >= W_HEIGHT / SECTOR_RANGE) {
				continue;
			}
			const auto& current_sector = sectors.sectors[sector_y][sector_x];
			for (const auto& player_id : current_sector) {
				const auto& player = clients[player_id];
				if (player.state != ST_INGAME) continue;
				if (true == is_npc(player.id)) continue;
				if (!can_see(player.id, npc_id)) continue;
				new_vl.insert(player_id);
			}
		}
	}

	for (auto pl : new_vl) {
		if (0 == old_viewlist.count(pl)) {
			// 플레이어의 시야에 등장
			clients[pl].send_add_player_packet(npc.id);
		}
		else {
			// 플레이어가 계속 보고 있음.
			clients[pl].send_move_packet(npc.id);
			//만약 NPC가 캐릭터 옆에 있다면? -> 때려
			if (can_attack(clients[pl].id, npc.id)) {
				bool old_state = false;
				atomic_compare_exchange_strong(&npc.can_attack, &old_state, true);
				TIMER_EVENT event{ npc.id, chrono::system_clock::now(), EV_NPC_ATTACK, clients[pl].id };
				timer_queue.push(event);
			}
		}
	}

	for (auto pl : old_viewlist) {
		if (0 == new_vl.count(pl)) {
			clients[pl].vl.lock();
			if (0 != clients[pl].view_list.count(npc.id)) {
				clients[pl].vl.unlock();
				clients[pl].send_remove_player_packet(npc.id);
			}
			else {
				clients[pl].vl.unlock();
			}
		}
	}

}

void do_npc_attack_player(int player_id)
{
	clients[player_id].npc_attack_lock.lock();
	clients[player_id].hp -= PLAYER_ATTACKED_DAMAGE;
	clients[player_id].npc_attack_lock.unlock();

	clients[player_id].send_npc_attack_player_packet();
}

void WakeupNPC(int npc_id, int waker)
{
	OVER_EXP* exover = new OVER_EXP;
	exover->comp_type = OP_AI_HELLO;
	exover->ai_target_obj = waker;
	PostQueuedCompletionStatus(h_iocp, 1, npc_id, &exover->over);

	if (clients[npc_id].is_active) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&clients[npc_id].is_active, &old_state, true))
		return;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_RANDOM_MOVE, 0 };
	timer_queue.push(ev);
}

int API_get_x(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = clients[user_id].x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = clients[user_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	clients[user_id].send_chat_packet(my_id, mess);
	return 0;
}

void InitializeNPC()
{
	std::cout << "NPC intialize begin.\n";
	for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
		while (true) {
			clients[i].x = rand() % W_WIDTH;
			clients[i].y = rand() % W_HEIGHT;
			if (false == is_collision(clients[i].x, clients[i].y)) {
				auto& client = clients[i];
				sectors.AddPlayerInSector(i, sectors.GetMySector_X(client.x), sectors.GetMySector_Y(client.y));
				break;
			}
		}
		clients[i].id = i;
		sprintf_s(clients[i].name, "NPC%d", i);
		clients[i].state = ST_INGAME;
		clients[i].is_active = false;
		clients[i].is_move = false;
		clients[i].player_die = false;
		clients[i].can_attack = false;
		clients[i].hp = MAX_HP;
		clients[i].prev_my_sector_x = sectors.GetMySector_X(clients[i].x);
		clients[i].prev_my_sector_y = sectors.GetMySector_Y(clients[i].y);
		clients[i].now_my_sector_x = sectors.GetMySector_X(clients[i].x);
		clients[i].now_my_sector_y = sectors.GetMySector_Y(clients[i].y);

		auto L = clients[i].L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "npc.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0);
		// lua_pop(L, 1);// eliminate set_uid from stack after call

		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
	std::cout << "NPC initialize end.\n";
}
