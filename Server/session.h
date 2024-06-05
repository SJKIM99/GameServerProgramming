#pragma once

#include "global.h"
#include "sector.h"

class OVER_EXP {
public:
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	char send_buf[BUF_SIZE];
	COMP_TYPE comp_type;
	int ai_target_obj;

	OVER_EXP()
	{
		wsabuf.buf = send_buf;
		wsabuf.len = BUF_SIZE;
		comp_type = OP_RECV;
		ZeroMemory(&over, sizeof(over));
	}
	OVER_EXP(char* packet)
	{
		wsabuf.buf = send_buf;
		wsabuf.len = packet[0];
		ZeroMemory(&over, sizeof(over));
		comp_type = OP_SEND;
		memcpy(send_buf, packet, packet[0]);
	}
};

extern OVER_EXP g_a_over;

class SESSION {
	OVER_EXP recv_over;
public:
	mutex s_lock;
	S_STATE state;
	atomic_bool is_active;
	int id;
	SOCKET socket;
	short x, y;
	char name[NAME_SIZE];
	unsigned last_move_time;
	lua_State* L;
	unsigned last_attack_time;
	int prev_remain;
	unordered_set<int> view_list;
	mutex vl;
	mutex chat_lock;
	mutex _ll;
	bool is_move;
	int level;
	int exp;
	int hp;
	int max_hp;
	atomic_bool can_attack;
	bool player_die;
	int player_offense;
	atomic_bool npc_attack;
	//이전섹터
	int prev_my_sector_x;
	int prev_my_sector_y;
	//이후섹터
	int now_my_sector_x;
	int now_my_sector_y;
	mutex sector_lock;
	mutex timer_lock;
	mutex npc_die_lock;
	mutex npc_attack_lock;
public:
	SESSION()
	{
		id = -1;
		socket = 0;
		x = y = 0;
		name[0] = 0;
		state = ST_FREE;
		prev_remain = 0;
		level = 0;
		exp = 0;
		hp = 0;
		max_hp = 0;
		player_offense = 0;
		prev_my_sector_x = -1;
		prev_my_sector_y = -1;
	}
	~SESSION() {}

	void do_recv();
	void do_send(void* packet);
	void send_login_info_packet();
	void send_move_packet(int c_id);
	void send_add_player_packet(int c_id);
	void send_chat_packet(int c_id, const char* mess);
	void send_remove_player_packet(int c_id);
	void send_login_fail_packet();
	void send_state_change_packet(int c_id);
	void send_player_die_packet(int c_id);
	void send_pc_die_packet(int pc_id);
	void send_heal_packet();
	void send_npc_die_packet(int npc_id);
	void send_npc_respawn_packet(int npc_id);
	void send_npc_attack_player_packet();
};

void disconnect(int c_id);
bool can_see(int from, int to);
int get_new_client_id();
bool can_attack(int from, int to);
