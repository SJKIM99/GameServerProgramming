#pragma once

#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <set>
#include <cmath> 
#include <concurrent_priority_queue.h>
#include "protocol.h"
#include <sqlext.h>
#include <cwchar>
#include <unordered_map>

#include "include/lua.hpp"


#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")


#define NAME_LEN 20
#define MAXHP 300
constexpr int  BUF_SIZE = 1024;

using namespace std;

constexpr int NAMESIZE = 20;
constexpr int IDSIZE = 20;
constexpr int PASSWDSIZE = 20;

constexpr int VIEW_RANGE = 5;
constexpr int ATTACK_RANGE = 1;
constexpr int SECTOR_RANGE = 10;
constexpr int MAX_HP = 100;
constexpr int HEAL_AMOUNT = 20;
constexpr int PLAYER_OFFENSIVE = 50;
constexpr int PLAYER_ATTACKED_DAMAGE = 10;

extern HANDLE h_iocp;

class sector;
class SESSION;
class OVER_EXP;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_NPC_MOVE, OP_AI_HELLO, OP_AI_BYE, OP_NPC_ATTACK, OP_HEAL, OP_PLAYER_RESPAWN, OP_SAVE_PLAYER_INFO, OP_GET_PLAYER_INFO, OP_ADD_PAYER_INFO, OP_NPC_RESPAWN };
enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
enum EVENT_TYPE { EV_RANDOM_MOVE, EV_BYE, EV_NPC_ATTACK, EV_HEAL, EV_PLAYER_RESPAWN, EV_NPC_RESPAWN };

struct TIMER_EVENT {
	int object_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event;
	int target_id;
	constexpr bool operator < (const TIMER_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};

enum DB_EVENT_TYPE { EV_LOGIN_PLAYER, EV_SAVE_PLAYER_INFO };

struct DB_PLAYER_INFO {
	char nickname[NAMESIZE];
	int x;
	int y;
};

struct DB_EVENT {
	int player_id;
	std::chrono::system_clock::time_point wakeup_time;
	DB_EVENT_TYPE event;
	DB_PLAYER_INFO player_info;
	constexpr bool operator < (const DB_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};

extern SOCKET g_s_socket, g_c_socket;
extern concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
extern concurrency::concurrent_priority_queue<DB_EVENT> m_database_queue;
extern array<SESSION, MAX_USER + MAX_NPC> clients;
extern int g_left_x;
extern int g_top_y;
extern sector sectors;
