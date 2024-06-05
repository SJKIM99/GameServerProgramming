#include "collision.h"
#include "global.h"

constexpr int SCREEN_WDITH = 16;
constexpr int SCREEN_HEIGHT = 16;

bool is_collision(int x_pos, int y_pos)
{
	g_left_x = x_pos;
	g_top_y = y_pos;

	for (int i = 0; i < SCREEN_WDITH; ++i) {
		for (int j = 0; j < SCREEN_HEIGHT; ++j) {
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (0 == (tile_x / 3 + tile_y / 3) % 3) {
				return false;
			}
			else if (1 == (tile_x / 3 + tile_y / 3) % 3)
			{
				return false;
			}
			else {	//��ֹ� �浹 ó�� �ؾ���
				if (0 == (tile_x / 2 + tile_y / 2) % 3) {
					return true;
				}
				else if (1 == (tile_x / 2 + tile_y / 2) % 3) {
					return false;
				}
				else {
					return false;
				}
			}
		}
	}
}

//Ŭ�󿡼� �Ǵ��ϴ� g_left_x, g_top_y
//
//g_left_x = my_packet->x - SCREEN_WIDTH / 2;
//g_top_y = my_packet->y - SCREEN_HEIGHT / 2;

//GUI�� �׸� ��
//if (2 == (tile_x / 3 + tile_y / 3) % 3)
//	if(0 == (tile_x / 2 + tile_y / 2) % 3) {
//		obstacle.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
//		obstacle.a_draw();
//		}
//	}

//Ŭ�󿡼� g_left_x, g_top_y ������ 8�� ���ϱ�
//������ �������� �浹 ó���� ������ 8�� ���ؼ� ��� �ؾ� ��
//