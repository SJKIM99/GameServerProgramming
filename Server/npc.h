#pragma once


bool is_pc(int obj_id);
bool is_npc(int object_id);
void do_npc_random_move(int npc_id);
void do_npc_attack_player(int player_id);
void WakeupNPC(int npc_id, int waker);
int API_get_x(lua_State* L);
int API_get_y(lua_State* L);
int API_SendMessage(lua_State* L);
void InitializeNPC();
