#include "global.h"
#include "session.h"
#include "npc.h"
#include "collision.h"
#include "sector.h"


HANDLE h_iocp;
SOCKET g_s_socket;
SOCKET g_c_socket;
concurrency::concurrent_priority_queue<TIMER_EVENT> timer_queue;
concurrency::concurrent_priority_queue<DB_EVENT> m_database_queue;
sector sectors;
OVER_EXP g_a_over;
array<SESSION, MAX_USER + MAX_NPC> clients;
int g_left_x;
int g_top_y;

array<int, 10> request_exp = { 100,200,400,800,1600,3200,6400,12800,25600,51200 };
SQLHENV m_henv = NULL;
SQLHDBC m_hdbc = NULL;
SQLHSTMT m_hstmt = NULL;
SQLRETURN m_retcode;

void show_error(SQLHANDLE handle, SQLSMALLINT handleType, RETCODE retcode)
{
	SQLSMALLINT i = 0;
	SQLINTEGER native;
	SQLWCHAR state[7];
	SQLWCHAR text[256];
	SQLSMALLINT len;
	SQLRETURN ret;

	do {
		ret = SQLGetDiagRec(handleType, handle, ++i, state, &native, text, sizeof(text), &len);
		if (SQL_SUCCEEDED(ret)) {
			std::cerr << "Message: " << text << "\nSQLSTATE: " << state << "\n";
		}
	} while (ret == SQL_SUCCESS);
}

bool DBConnect()
{
	m_retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);

	// Set the ODBC version environment attribute  
	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		m_retcode = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);
	}
	else {
		return false;
	}

	// Allocate connection handle  
	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		m_retcode = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);
	}
	else {
		return false;
	}

	// Set login timeout to 5 seconds  
	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		SQLSetConnectAttr(m_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
	}
	else {
		return false;
	}

	// Connect to data source  
	m_retcode = SQLConnect(m_hdbc, (SQLWCHAR*)L"DB_TermProject", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

	// Allocate statement handle  
	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		m_retcode = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &m_hstmt);
	}
	else {
		return false;
	}
	cout << "DB Connect Success" << endl;
	return true;
}

bool DBDisConnect()
{
	SQLDisconnect(m_hdbc);

	SQLFreeHandle(SQL_HANDLE_STMT, m_hstmt);
	SQLFreeHandle(SQL_HANDLE_STMT, m_hdbc);
	SQLFreeHandle(SQL_HANDLE_STMT, m_henv);

	m_henv = NULL;
	m_hdbc = NULL;
	m_hstmt = NULL;
	m_retcode = 0;

	return true;
}

bool IsPlayerRegistered(std::string NICKNAME)
{
	SQLCHAR isRegistered{};
	SQLLEN cb_isRegistered{};
	
	std::wstring sqlQuery = L"EXEC isPlayerRegistered ?";
	m_retcode = SQLPrepare(m_hstmt, (SQLWCHAR*)sqlQuery.c_str(), SQL_NTS);
	m_retcode = SQLBindParameter(m_hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, NAMESIZE, 0, (SQLPOINTER)NICKNAME.c_str(), 0, NULL);
	m_retcode = SQLExecute(m_hstmt);
	if (m_retcode == -1)show_error(m_hstmt, SQL_HANDLE_STMT, m_retcode);
	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		m_retcode = SQLBindCol(m_hstmt, 1, SQL_BIT, &isRegistered, sizeof(isRegistered), &cb_isRegistered);
	}
	
	m_retcode = SQLFetch(m_hstmt);

	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		SQLFreeStmt(m_hstmt, SQL_RESET_PARAMS);  // 매개변수 재설정
		SQLCloseCursor(m_hstmt);  // 문장과 연결된 커서 닫기
		return (isRegistered == 1);
	}
	else {
		return false;
	}
}

PlayerInfo ExtractPlayerInfo(string NICKNAME)
{
	PlayerInfo playerinfo;

	SQLINTEGER player_x{}, player_y{};
	SQLLEN cb_x{}, cb_y{};

	std::wstring sqlQuery = L"EXEC ExtractPlayerInfo ?";
	m_retcode = SQLPrepare(m_hstmt, (SQLWCHAR*)sqlQuery.c_str(), SQL_NTS);
	m_retcode = SQLBindParameter(m_hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0, (SQLPOINTER)NICKNAME.c_str(), 0, NULL);
	m_retcode = SQLExecute(m_hstmt);
	// Bind columns 1
	m_retcode = SQLBindCol(m_hstmt, 1, SQL_INTEGER, &player_x, NAMESIZE, &cb_x);
	m_retcode = SQLBindCol(m_hstmt, 2, SQL_INTEGER, &player_y, NAMESIZE, &cb_y);

	m_retcode = SQLFetch(m_hstmt);

	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		SQLFreeStmt(m_hstmt, SQL_RESET_PARAMS);  // 매개변수 재설정
		SQLCloseCursor(m_hstmt);  // 문장과 연결된 커서 닫기
		playerinfo.x = player_x;
		playerinfo.y = player_y;
		return playerinfo;
	}
}

void SavePlayerInfo(SESSION& player)
{
	SQLINTEGER player_x = player.x;
	SQLINTEGER player_y = player.y;
	string nickname = player.name;

	std::wstring sqlQuery = L"EXEC SavePlayerInfo ?, ?, ?";
	m_retcode = SQLPrepare(m_hstmt, (SQLWCHAR*)sqlQuery.c_str(), SQL_NTS);

	// Bind parameters
	m_retcode = SQLBindParameter(m_hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, NAMESIZE, 0, (SQLPOINTER)nickname.c_str(), 0, NULL);
	m_retcode = SQLBindParameter(m_hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &player_x, 0, NULL);
	m_retcode = SQLBindParameter(m_hstmt, 3, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &player_y, 0, NULL);

	m_retcode = SQLExecute(m_hstmt);
	if (m_retcode == SQL_SUCCESS || m_retcode == SQL_SUCCESS_WITH_INFO) {
		SQLFreeStmt(m_hstmt, SQL_CLOSE);
		SQLFreeStmt(m_hstmt, SQL_UNBIND);
		SQLFreeStmt(m_hstmt, SQL_RESET_PARAMS);
		SQLCloseCursor(m_hstmt);
	}
}

void process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		PlayerInfo login_player{};

		if (IsPlayerRegistered(p->name)) {	//로그인 정보가 있다면? -> 꺼내오기
			login_player = ExtractPlayerInfo(p->name);
			{
				lock_guard<mutex> ll{ clients[c_id].s_lock };
				strcpy_s(clients[c_id].name, p->name);
				clients[c_id].x = login_player.x;
				clients[c_id].y = login_player.y;
				clients[c_id].state = ST_INGAME;
				clients[c_id].prev_my_sector_x = sectors.GetMySector_X(clients[c_id].x);
				clients[c_id].prev_my_sector_y = sectors.GetMySector_Y(clients[c_id].y);
				clients[c_id].now_my_sector_x = sectors.GetMySector_X(clients[c_id].x);
				clients[c_id].now_my_sector_y = sectors.GetMySector_Y(clients[c_id].y);
				clients[c_id].max_hp = MAX_HP;
				clients[c_id].hp = MAX_HP / 2;
		
				sectors.AddPlayerInSector(c_id, sectors.GetMySector_X(clients[c_id].x), sectors.GetMySector_Y(clients[c_id].y));
			}
		}
		else {														//없다면 새로 만들어주기
			strcpy_s(clients[c_id].name, p->name);
			{
				lock_guard<mutex> ll{ clients[c_id].s_lock };
				clients[c_id].x = rand() % W_WIDTH;
				clients[c_id].y = rand() % W_HEIGHT;
				clients[c_id].state = ST_INGAME;
				clients[c_id].prev_my_sector_x = sectors.GetMySector_X(clients[c_id].x);
				clients[c_id].prev_my_sector_y = sectors.GetMySector_Y(clients[c_id].y);
				clients[c_id].now_my_sector_x = sectors.GetMySector_X(clients[c_id].x);
				clients[c_id].now_my_sector_y = sectors.GetMySector_Y(clients[c_id].y);
				clients[c_id].max_hp = MAX_HP;

				sectors.AddPlayerInSector(c_id, sectors.GetMySector_X(clients[c_id].x), sectors.GetMySector_Y(clients[c_id].y));
			}
		}
		clients[c_id].send_login_info_packet();
		for (auto& pl : clients) {
			{
				lock_guard<mutex> ll(pl.s_lock);
				if (ST_INGAME != pl.state) continue;
			}
			if (pl.id == c_id) continue;
			if (false == can_see(c_id, pl.id))
				continue;
			if (is_pc(pl.id)) pl.send_add_player_packet(c_id);
			else WakeupNPC(pl.id, c_id);
			clients[c_id].send_add_player_packet(pl.id);
		}
		//로그인시 체력회복 timer on
		TIMER_EVENT event{ c_id, chrono::system_clock::now() + 5s, EV_HEAL, 0 };
		timer_queue.push(event);
	}
		break;
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);

		unsigned now_time = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());

		if (now_time > clients[c_id].last_move_time + 1000) {
			clients[c_id].last_move_time = p->move_time;
			short x = clients[c_id].x;
			short y = clients[c_id].y;

			switch (p->direction) {
			case 0: {
				if (y > 0) {
					--y;
					if (!is_collision(x, y)) y;
					else ++y;
				}
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
			clients[c_id].sector_lock.lock();
			if (sectors.UpdatePlayerInSector(c_id, sectors.GetMySector_X(x), sectors.GetMySector_Y(y), sectors.GetMySector_X(clients[c_id].x), sectors.GetMySector_Y(clients[c_id].y))) {
				clients[c_id].prev_my_sector_x = sectors.GetMySector_X(clients[c_id].x);
				clients[c_id].prev_my_sector_y = sectors.GetMySector_Y(clients[c_id].y);
				clients[c_id].now_my_sector_x = sectors.GetMySector_X(x);
				clients[c_id].now_my_sector_y = sectors.GetMySector_X(y);
			}
			clients[c_id].x = x;
			clients[c_id].y = y;
			clients[c_id].sector_lock.unlock();

			clients[c_id].send_move_packet(c_id);

			//DB에 좌표 업데이트
			DB_EVENT player_update_event{ c_id,chrono::system_clock::now(), EV_SAVE_PLAYER_INFO };
			m_database_queue.push(player_update_event);

			unordered_set<int> near_list;

			clients[c_id].vl.lock();
			unordered_set<int> old_viewlist = clients[c_id].view_list;
			clients[c_id].vl.unlock();

			//c_id가 속한 섹터를 찾아 섹터를 순회한다.
			auto& my_player = clients[c_id];

			// 현재 섹터와 주변 8개의 섹터를 순회한다.
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					int sector_y = my_player.now_my_sector_y + dy;
					int sector_x = my_player.now_my_sector_x + dx;
					// 유효한 섹터 범위인지 확인 
					if (sector_y < 0 || sector_y >= W_WIDTH / SECTOR_RANGE || sector_x < 0 || sector_x >= W_HEIGHT / SECTOR_RANGE) {
						continue;
					}
					const auto& current_sector = sectors.sectors[sector_y][sector_x];
					for (const auto& player_id : current_sector) {
						const auto& player = clients[player_id];
						if (player.state != ST_INGAME) continue;
						if (player.id == c_id) continue;
						if (!can_see(player.id, c_id)) continue;
						near_list.insert(player_id);
					}
				}
			}

			for (auto& cl : near_list) {	//near_list에는 이제 c_id 시야안에 있는 클라이언트 들이야 (플레이어 +NPC)
				auto& player = clients[cl];
				if (is_pc(cl)) {	//player일 경우
					player.vl.lock();
					if (clients[cl].view_list.count(c_id)) {	//cl번 플레이어 뷰리스트 안에 c_id가 있니?
						player.vl.unlock();
						clients[cl].send_move_packet(c_id);
					}
					else {	
						player.vl.unlock();
						clients[cl].send_add_player_packet(c_id);
					}
				}
				else {	//NPC일 경우
					WakeupNPC(cl, c_id);
				}
				if (old_viewlist.count(cl) == 0) {
					clients[c_id].send_add_player_packet(cl);
				}
			}

			for (auto& cl : old_viewlist) {
				if (0 == near_list.count(cl)) {
					clients[c_id].send_remove_player_packet(cl);
					if (is_pc(cl)) {
						clients[cl].send_remove_player_packet(c_id);
					}
				}
			}
		}
	}
	break;
	case CS_ATTACK: {
		CS_ATTACK_PACKET* p = reinterpret_cast<CS_ATTACK_PACKET*>(packet);
		unsigned now_time = static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
		if (now_time > clients[c_id].last_attack_time + 1000) {
			clients[c_id].last_attack_time = p->attack_time;
			auto& attack_player = clients[c_id];

			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					int sector_y = attack_player.now_my_sector_y + dy;
					int sector_x = attack_player.now_my_sector_x + dx;
					if (sector_y < 0 || sector_y >= W_WIDTH / SECTOR_RANGE || sector_x < 0 || sector_x >= W_HEIGHT / SECTOR_RANGE) {
						continue;
					}
					const auto& current_sector = sectors.sectors[sector_y][sector_x];
					for (const auto& npc_id : current_sector) {
						const auto& npc = clients[npc_id];
						if (npc.state != ST_INGAME) continue;
						if (!is_npc(npc.id)) continue;
						if (npc.id == c_id) continue;
						if (!can_attack(npc.id, c_id)) continue;
						clients[npc_id].hp -= PLAYER_OFFENSIVE;
						if (clients[npc_id].hp == 0) {	//사망
							{
								lock_guard<mutex> ll(clients[npc_id].s_lock);
								clients[npc_id].state = ST_FREE;
							}
							attack_player.send_npc_die_packet(npc_id);
							///추가적으로 플레이어 npc 렌덤 위치에 스폰해야해
							//타이머큐에서는 이제 다른 플레이어에게 안보여야 하니까 상태를 ST_FREE로 바꾸기
							//그후 PQCS실행되면 ST_INGAME으로 바꾸고 섹터 설정 및 초기 설정 해주기
							TIMER_EVENT event{ npc_id, chrono::system_clock::now() + 10s, EV_NPC_RESPAWN, 0 };
							timer_queue.push(event);
						}
					}
				}
			}
		}
	}
				  break;
	}
}

void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;

		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);

		if (FALSE == ret) {
			if (ex_over->comp_type == OP_ACCEPT) std::cout << "Accept Error";
			else {
				std::cout << "GQCS Error on CLient[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->comp_type == OP_RECV) || (ex_over->comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> state_lock{ clients[client_id].s_lock };
					clients[client_id].state = ST_ALLOC;
				}
				clients[client_id].x = 0;
				clients[client_id].y = 0;	
				clients[client_id].id = client_id;
				clients[client_id].prev_remain = 0;
				clients[client_id].socket = g_c_socket;
				clients[client_id].hp = 0;
				clients[client_id].max_hp = 0;
				clients[client_id].level = 0;
				clients[client_id].exp = 0;
				clients[client_id].last_move_time = 0;
				clients[client_id].last_attack_time = 0;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				clients[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				std::cout << "MAX user exceeded.\n";
			}
			ZeroMemory(&g_a_over.over, sizeof(g_a_over.over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over);
		}
					  break;
		case OP_RECV: {
			int remain_data = num_bytes + clients[key].prev_remain;
			char* p = ex_over->send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			clients[key].prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->send_buf, p, remain_data);
			}
			clients[key].do_recv();
		}
					break;
		case OP_SEND: {
			delete ex_over;
		}
					break;
		case OP_NPC_MOVE: {
			if (clients[key].can_attack == false) {
				bool keep_alive = false;
				for (int j = 0; j < MAX_USER; ++j) {
					if (clients[j].state != ST_INGAME) continue;
					if (can_see(static_cast<int>(key), j)) {
						keep_alive = true;
						break;
					}
				}
				if (true == keep_alive) {
					do_npc_random_move(static_cast<int>(key));
					TIMER_EVENT ev{ key, chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
					timer_queue.push(ev);
				}
				else {
					clients[key].is_active = false;
				}
			}
			delete ex_over;
		}
						break;
		case OP_AI_HELLO: {
			clients[key]._ll.lock();
			auto L = clients[key].L;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, ex_over->ai_target_obj);
			lua_pcall(L, 1, 0, 0);
			//lua_pop(L, 1);
			clients[key]._ll.unlock();
			delete ex_over;
		}
						break;
		case OP_NPC_ATTACK: {
			if (clients[key].state != ST_INGAME) {
				delete ex_over;
				break;
			}
			bool keep_attack = false;
			int player_id{};
			for (int j = 0; j < MAX_USER; ++j) {
				if (clients[j].state != ST_INGAME) continue;
				if (can_attack(static_cast<int>(key), j)) {
					player_id = j;
					keep_attack = true;
					break;
				}
			}
			if (true == keep_attack) {
				do_npc_attack_player(player_id);
				TIMER_EVENT ev{ key, chrono::system_clock::now()+1s, EV_NPC_ATTACK, 0 };
				timer_queue.push(ev);
			}
			else {
				clients[key].can_attack = false;
			}
			delete ex_over;
		}
						  break;
		case OP_HEAL: {
			if (clients[key].hp + HEAL_AMOUNT >= MAX_HP) {
				clients[key].hp = MAX_HP;
			}
			else {
				clients[key].hp += HEAL_AMOUNT;
			}
			clients[key].send_heal_packet();

			TIMER_EVENT ev{ key, chrono::system_clock::now() + 5s, EV_HEAL, 0 };
			timer_queue.push(ev);
		}
					break;
		case OP_NPC_RESPAWN: {	
			cout << key << "번 NPC 리스폰" << endl;
			auto& respawn_npc = clients[key];

			respawn_npc.s_lock.lock();
			respawn_npc.state = ST_INGAME;
			while (true) {
				respawn_npc.x = rand() % W_WIDTH;
				respawn_npc.y = rand() % W_HEIGHT;
				if (false == is_collision(respawn_npc.x, respawn_npc.y)) {
					sectors.AddPlayerInSector(key, sectors.GetMySector_X(respawn_npc.x), sectors.GetMySector_Y(respawn_npc.y));
					respawn_npc.prev_my_sector_x = sectors.GetMySector_X(respawn_npc.x);
					respawn_npc.prev_my_sector_y = sectors.GetMySector_Y(respawn_npc.y);
					respawn_npc.now_my_sector_x = sectors.GetMySector_X(respawn_npc.x);
					respawn_npc.now_my_sector_y = sectors.GetMySector_Y(respawn_npc.y);
					break;
				}
			}
			respawn_npc.s_lock.unlock();
			//클라이언트들에게 리스폰됐다고 알려주기
			//NPC가 스폰된 주변 섹터에 있는 플레이어들에게만 알리면 안될까?
			//어짜피 섹터에는 스폰된 NPC의 아이디가 있잖아 -> 그러면 가능하네
			for (int dy = -1; dy <= 1; ++dy) {
				for (int dx = -1; dx <= 1; ++dx) {
					int sector_y = respawn_npc.now_my_sector_y + dy;
					int sector_x = respawn_npc.now_my_sector_x + dx;
					if (sector_y < 0 || sector_y >= W_WIDTH / SECTOR_RANGE || sector_x < 0 || sector_x >= W_HEIGHT / SECTOR_RANGE) {
						continue;
					}
					const auto& current_sector = sectors.sectors[sector_y][sector_x];
					for (const auto& player_id : current_sector) {
						auto& player = clients[player_id];
						if (player.state != ST_INGAME) continue;
						if (is_npc(player.id)) continue;
						if (!can_see(player_id, respawn_npc.id)) continue;
						//여기에 NPC리스폰되었다는 패킷 보내기
						player.send_npc_respawn_packet(respawn_npc.id);
					}
				}
			}

		}
						   break;
		}
	}
}

void do_timer()
{
	while (true) {
		TIMER_EVENT event;
		auto current_time = chrono::system_clock::now();
		if (true == timer_queue.try_pop(event)) {
			if (event.wakeup_time > current_time) {
				timer_queue.push(event);
				this_thread::sleep_for(1ms);
				continue;
			}

			switch (event.event) {
			case EV_RANDOM_MOVE: {
				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, event.object_id, &ov->over);
			}
							   break;
			case EV_NPC_ATTACK: {
				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type = OP_NPC_ATTACK;
				PostQueuedCompletionStatus(h_iocp, 1, event.object_id, &ov->over);
			}
							  break;
			case EV_HEAL: {
				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type = OP_HEAL;
				PostQueuedCompletionStatus(h_iocp, 1, event.object_id, &ov->over);
			}
						break;
			case EV_NPC_RESPAWN: {
				auto& respawn_npc = clients[event.object_id];
				sectors.RemovePlayerInSector(event.object_id, sectors.GetMySector_X(respawn_npc.x), sectors.GetMySector_Y(respawn_npc.y));

				OVER_EXP* ov = new OVER_EXP;
				ov->comp_type = OP_NPC_RESPAWN;
				PostQueuedCompletionStatus(h_iocp, 1, event.object_id, &ov->over);
			}
							   break;
			}
		}
		this_thread::sleep_for(1ms);
	}
}


void do_database()
{
	while (true) {
		DB_EVENT ev{};
		auto current_time = std::chrono::system_clock::now();
		if (true == m_database_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				m_database_queue.push(ev);
				this_thread::sleep_for(1ms);
				continue;
			}
		}
		switch (ev.event) {
		case EV_GET_PLAYER_INFO: {

		}
							   break;
		case EV_SAVE_PLAYER_INFO: {
			SavePlayerInfo(clients[ev.player_id]);
		}
								break;
		}
		this_thread::sleep_for(1ms);
	}
}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);

	DBConnect();

	InitializeNPC();

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over.comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over.send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over.over);


	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency() - 2;
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);

	thread timer_thread{ do_timer };

	thread db_thread{ do_database };

	for (auto& th : worker_threads)
		th.join();
	timer_thread.join();
	db_thread.join();

	closesocket(g_s_socket);
	WSACleanup();
}