#ifndef _BRANCH_H_
#define _BRANCH_H_

#include "utils.h"

extern int branch_border_on_diamond_left[IMG_H];
extern int branch_border_on_diamond_right[IMG_H];

typedef enum {
    BRANCH_STATUS_NONE = 0,
    BRANCH_STATUS_IN,
    BRANCH_STATUS_IN_STABLE,
    BRANCH_STATUS_STAY,
    BRANCH_STATUS_OUT,
    BRANCH_STATUS_OUT_STABLE,
} branch_status_t;

extern branch_status_t branch_status;

extern pair_t branch_corner_bottom_right, branch_corner_top_right, branch_corner_in;

extern int branch_border_left[IMG_H]; // 没有边界时为 -1
extern int branch_border_right[IMG_H]; // 没有边界时为 -1
extern int branch_guide[IMG_H]; // 没有边界时为 -1

void branch_status_set_in(void); // 识别三岔进入时调用该函数

void branch_main(void);

#endif