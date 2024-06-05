#include "global.h"
#include "npc.h"
#include "session.h"

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&recv_over.over, 0, sizeof(recv_over.over));
	recv_over.wsabuf.len = BUF_SIZE - prev_remain;
	recv_over.wsabuf.buf = recv_over.send_buf + prev_remain;
	WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.over, 0);
}

void SESSION::do_send(void* packet)
{
	OVER_EXP* send_data = new OVER_EXP{ reinterpret_cast<char*>(packet) };
	WSASend(socket, &send_data->wsabuf, 1, 0, 0, &send_data->over, 0);
}

void SESSION::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET packet;
	packet.id = id;
	packet.size = sizeof SC_LOGIN_INFO_PACKET;
	packet.type = SC_LOGIN_INFO;
	packet.x = x;
	packet.y = y;
	packet.max_hp = max_hp;
	packet.hp = hp;
	packet.level = level;
	packet.exp = exp;

	do_send(&packet);
}

void SESSION::send_move_packet(int c_id)
{
	SC_MOVE_OBJECT_PACKET packet;
	packet.id = c_id;
	packet.size = sizeof SC_MOVE_OBJECT_PACKET;
	packet.type = SC_MOVE_OBJECT;
	packet.x = clients[c_id].x;
	packet.y = clients[c_id].y;
	packet.move_time = clients[c_id].last_move_time;
	do_send(&packet);
}

void SESSION::send_add_player_packet(int c_id)
{
	vl.lock();
	view_list.insert(c_id);
	vl.unlock();

	SC_ADD_OBJECT_PACKET packet;
	packet.id = c_id;
	strcpy_s(packet.name, clients[c_id].name);
	packet.size = sizeof packet;
	packet.type = SC_ADD_OBJECT;
	packet.x = clients[c_id].x;
	packet.y = clients[c_id].y;
	do_send(&packet);
}

void SESSION::send_chat_packet(int p_id, const  char* mess)
{
	SC_CHAT_PACKET packet;
	packet.id = p_id;
	packet.size = sizeof(SC_CHAT_PACKET);
	packet.type = SC_CHAT;
	strcpy_s(packet.mess, mess);
	do_send(&packet);
}

void SESSION::send_remove_player_packet(int c_id)
{
	vl.lock();
	if (view_list.count(c_id)) {
		view_list.erase(c_id);
	}
	else {
		vl.unlock();
		return;
	}
	vl.unlock();

	SC_REMOVE_OBJECT_PACKET packet;
	packet.id = c_id;
	packet.size = sizeof packet;
	packet.type = SC_REMOVE_OBJECT;
	do_send(&packet);
}

void disconnect(int c_id)
{
	clients[c_id].vl.lock();
	unordered_set<int> viewlist = clients[c_id].view_list;
	clients[c_id].vl.unlock();
	for (auto& p_id : viewlist) {
		if (is_npc(p_id)) continue;
		auto& player = clients[p_id];
		{
			lock_guard<mutex> ll(player.s_lock);
			if (ST_INGAME != player.state) continue;
		}
		if (player.id == c_id) continue;
		player.send_remove_player_packet(c_id);
	}
	closesocket(clients[c_id].socket);

	lock_guard<mutex> ll(clients[c_id].s_lock);
	clients[c_id].state = ST_FREE;
}

void SESSION::send_login_fail_packet()
{
	SC_LOGIN_FAIL_PACKET packet;
	packet.size = sizeof packet;
	packet.type = SC_LOGIN_FAIL;
	do_send(&packet);
}

bool can_see(int from, int to)
{
	if (abs(clients[from].x - clients[to].x) > VIEW_RANGE) return false;
	return abs(clients[from].y - clients[to].y) <= VIEW_RANGE;
}

bool can_attack(int from, int to)
{
	if (abs(clients[from].x - clients[to].x) > ATTACK_RANGE) return false;
	return abs(clients[from].y - clients[to].y) <= ATTACK_RANGE;
}
int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		lock_guard <mutex> ll{ clients[i].s_lock };
		if (clients[i].state == ST_FREE)
			return i;
	}
	return -1;
}

void SESSION::send_state_change_packet(int c_id)
{
	SC_STAT_CHANGE_PACKET packet;
	packet.size = sizeof(SC_STAT_CHANGE_PACKET);
	packet.type = SC_STAT_CHANGE;
	packet.max_hp = max_hp;
	packet.hp = hp;
	packet.level = level;
	packet.exp = exp;
	do_send(&packet);
}


void SESSION::send_player_die_packet(int c_id)
{
	SC_NPC_DIE_PACKET packet;
	packet.size = sizeof(SC_NPC_DIE_PACKET);
	packet.type = SC_PLAYER_DIE;
	packet.npc_id = c_id;

	do_send(&packet);
}

void SESSION::send_pc_die_packet(int pc_id)
{
	SC_PC_DIE_PACKET packet;
	packet.size = sizeof(SC_PC_DIE_PACKET);
	packet.type = SC_PC_DIE;
	packet.id = pc_id;
	packet.x = x;
	packet.y = y;
	packet.hp = hp;
	packet.exp = exp;

	do_send(&packet);
}

void SESSION::send_heal_packet()
{
	SC_HEAL_PACKET packet;
	packet.size = sizeof(SC_HEAL_PACKET);
	packet.type = SC_HEAL;
	packet.hp = hp;

	do_send(&packet);
}

void SESSION::send_npc_die_packet(int npc_id)
{
	SC_NPC_DIE_PACKET packet;
	packet.size = sizeof(SC_NPC_DIE_PACKET);
	packet.type = SC_NPC_DIE;
	packet.npc_id = npc_id;

	cout << npc_id << "¹ø NPC »ç¸Á" << std::endl;
	do_send(&packet);
}

void SESSION::send_npc_respawn_packet(int npc_id)
{
	SC_NPC_RESPAWN_PACKET packet;
	packet.size = sizeof SC_NPC_RESPAWN_PACKET;
	packet.type = SC_NPC_RESPAWN;
	packet.npc_id = npc_id;
	packet.x = x;
	packet.y = y;

	do_send(&packet);
}

void SESSION::send_npc_attack_player_packet()
{
	SC_NPC_ATTACK_PLAYER_PACKET packet;
	packet.size = sizeof SC_NPC_ATTACK_PLAYER_PACKET;
	packet.type = SC_NPC_ATTACK_PLAYER;
	packet.id = id;
	packet.hp = hp;

	do_send(&packet);
}
