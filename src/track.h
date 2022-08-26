#ifndef _TRACK_H_
#define _TRACK_H_

#include "utils.h"

extern uint8_t track_img[IMG_H][IMG_W];

extern uint8_t track_not_border_img[IMG_H][IMG_W];

#define TRACK_BORDER_DIFF 30 // 边界周围灰度差
#define TRACK_BORDER_R 1 // 边界检测核半径

extern uint8_t track_connected_img[IMG_H][IMG_W];
extern int track_connected_cnt;

typedef struct {
    int id;
    int size;
    pair_t center;
    int color;
    bool invalid_flag, track_flag;
} track_connected_stat_t;

#define TRACK_CONNECTED_SIZE 254 // 少一个为了预防 +1 后溢出
extern track_connected_stat_t track_connected_stat[TRACK_CONNECTED_SIZE + 1];

extern track_connected_stat_t *track_connected_stat_sorted[TRACK_CONNECTED_SIZE + 1];

extern int track_white_id; // 为 0 代表不存在

#define TRACK_BORDER_SIZE 900 // 600 对于一般情况是够用了，但是对于斑马线情况（8号视频990帧左右）还是不够大会报assert错误

extern pair_t track_border[TRACK_BORDER_SIZE]; // 存储边界信息
extern int track_border_n;

typedef struct {
  bool invalid_flag;
} track_border_stat_t;

extern track_border_stat_t track_border_stat[TRACK_BORDER_SIZE];

extern int track_border_left[IMG_H];
extern int track_border_right[IMG_H];
extern int track_guide[IMG_H];

void track_main(void);

#endif