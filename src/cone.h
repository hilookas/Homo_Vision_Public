#ifndef _CONE_H_
#define _CONE_H_

#include "utils.h"

extern uint8_t cone_img[IMG_H][IMG_W];

extern uint8_t cone_thresholded_img[IMG_H][IMG_W];

extern uint8_t cone_connected_img[IMG_H][IMG_W];
extern int cone_connected_cnt;

typedef struct {
    int id;
    int size;
    pair_t center;
    int color;
    bool invalid_flag, cone_flag;
    pair_t bottom;
} cone_connected_stat_t;

#define CONE_CONNECTED_SIZE 254 // 少一个为了预防 +1 后溢出
extern cone_connected_stat_t cone_connected_stat[CONE_CONNECTED_SIZE + 1];

extern int track_border_bottom_id, track_border_top_id; // 存储封底和封顶点

extern cone_connected_stat_t *cone_connected_stat_sorted[CONE_CONNECTED_SIZE + 1];
extern int cone_connected_stat_order[CONE_CONNECTED_SIZE + 1];

typedef enum {
    CONE_STATUS_NONE = 0,
    CONE_STATUS_IN_PRE,
    CONE_STATUS_IN,
    CONE_STATUS_IN_AFTER,
    CONE_STATUS_STAY,
    CONE_STATUS_OUT,
    CONE_STATUS_OUT_AFTER,
} cone_status_t;

extern cone_status_t cone_status;

extern pair_t cone_entry2;
extern pair_t cone_exit2;

extern int cone_border_left[IMG_H]; // 没有边界时为 -1
extern int cone_border_right[IMG_H]; // 没有边界时为 -1
extern int cone_guide[IMG_H]; // 没有边界时为 -1

void cone_main(void);

#endif