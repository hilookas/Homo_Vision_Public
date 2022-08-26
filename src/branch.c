#include "branch.h"
#include "track.h"
#include <stdio.h>
#include <string.h>

// track_border_left 为最左的左边界（三岔外侧的边界）
int branch_border_on_diamond_left[IMG_H]; // 为最右的左边界（三岔菱形上的边界）
int branch_border_on_diamond_right[IMG_H];

static void branch_border_on_diamond_find(void) {
    if (track_white_id == 0) { // 没找到赛道
        // 保持上次赛道信息
        return;
    }

    memset(branch_border_on_diamond_left, 0xff, sizeof branch_border_on_diamond_left);
    memset(branch_border_on_diamond_right, 0xff, sizeof branch_border_on_diamond_right);

    for (int i = 0; i < track_border_n; ++i) {
        // 参见 track.c 的 track_not_border_find 函数
        // 等价于为灰度差增加了方向内容
        int a = track_border[i].a, b = track_border[i].b,
            a1 = a - TRACK_BORDER_R, a2 = a + TRACK_BORDER_R,
            b1 = b - TRACK_BORDER_R, b2 = b + TRACK_BORDER_R,
            b3 = b - TRACK_BORDER_R * 4, b4 = b + TRACK_BORDER_R * 4;

        // 扫最右的左边界
        if ((0 <= b1 && b1 < IMG_W && 0 <= b2 && b2 < IMG_W 
                && track_img[a][b2] - track_img[a][b1] >= TRACK_BORDER_DIFF // 右比左白
            || 0 <= b3 && b3 < IMG_W && 0 <= b4 && b4 < IMG_W 
                && track_img[a][b4] - track_img[a][b3] >= TRACK_BORDER_DIFF // 放宽判断区域，右比左白
        ) && (branch_border_on_diamond_left[a] == -1 // 没记录过
            || b > branch_border_on_diamond_left[a] // 最右的
        )) {
            branch_border_on_diamond_left[a] = b;
        }

        // 扫最左的右边界
        if ((0 <= b1 && b1 < IMG_W && 0 <= b2 && b2 < IMG_W 
                && track_img[a][b1] - track_img[a][b2] >= TRACK_BORDER_DIFF // 左比右白
            || 0 <= b3 && b3 < IMG_W && 0 <= b4 && b4 < IMG_W 
                && track_img[a][b3] - track_img[a][b4] >= TRACK_BORDER_DIFF // 放宽判断区域，左比右白
        ) && (branch_border_on_diamond_right[a] == -1 // 没记录过
            || b < branch_border_on_diamond_right[a] // 最左的
        )) {
            branch_border_on_diamond_right[a] = b;
        }
    }

    // 去除噪点的影响（滤波）
    for (int i = IMG_H - 1 - 1; i >= 0 + 1; --i) {
        if (branch_border_on_diamond_left[i - 1] != -1 && branch_border_on_diamond_left[i] != -1 && branch_border_on_diamond_left[i + 1] != -1
            && my_abs(branch_border_on_diamond_left[i - 1] - branch_border_on_diamond_left[i + 1]) < IMG_W * 0.1 // 上下两个点距离很近
            && my_abs(branch_border_on_diamond_left[i] - (branch_border_on_diamond_left[i - 1] + branch_border_on_diamond_left[i + 1]) / 2) > IMG_W * 0.2 // 但是中间那个点偏的离谱
        ) {
            branch_border_on_diamond_left[i] = (branch_border_on_diamond_left[i - 1] + branch_border_on_diamond_left[i + 1]) / 2;
        }
    }
    for (int i = IMG_H - 1 - 1; i >= 0 + 1; --i) {
        if (branch_border_on_diamond_right[i - 1] != -1 && branch_border_on_diamond_right[i] != -1 && branch_border_on_diamond_right[i + 1] != -1
            && my_abs(branch_border_on_diamond_right[i - 1] - branch_border_on_diamond_right[i + 1]) < IMG_W * 0.1 // 上下两个点距离很近
            && my_abs(branch_border_on_diamond_right[i] - (branch_border_on_diamond_right[i - 1] + branch_border_on_diamond_right[i + 1]) / 2) > IMG_W * 0.2 // 但是中间那个点偏的离谱
        ) {
            branch_border_on_diamond_right[i] = (branch_border_on_diamond_right[i - 1] + branch_border_on_diamond_right[i + 1]) / 2;
        }
    }
}

branch_status_t branch_status;

pair_t branch_corner_bottom_right, branch_corner_top_right, branch_corner_in;

int branch_border_left[IMG_H]; // 没有边界时为 -1
int branch_border_right[IMG_H]; // 没有边界时为 -1
int branch_guide[IMG_H]; // 没有边界时为 -1

static void branch_guide_update(void) {
    // 计算引导线（中线）
    for (int i = 0; i < IMG_H; ++i) {
        if (branch_border_left[i] != -1 && branch_border_right[i] != -1) {
            branch_guide[i] = (branch_border_left[i] + branch_border_right[i]) / 2;
        } else {
            branch_guide[i] = -1;
        }
    }
}

// static void branch_guide_update_from_border_left(void) {
//     // 左边界右移来实现引导线
//     for (int i = IMG_H - 1; i >= 0; --i) {
//         branch_guide[i] = my_min(branch_border_left[i] + my_track_width[i] / 2 * 1.1, IMG_W - 1);
//     }
// }

static void branch_guide_update_from_border_right(void) {
    // 右边界左移来实现引导线
    for (int i = IMG_H - 1; i >= 0; --i) {
        branch_guide[i] = my_max(branch_border_right[i] - my_track_width[i] / 2 * 1.1, 0);
    }
}

static void branch_guide_update_from_border_right_without_gap(void) {
    // 右边界左移来实现引导线
    for (int i = IMG_H - 1; i >= 0; --i) {
        branch_guide[i] = my_max(branch_border_right[i] - my_track_width[i] / 2, 0);
    }
}

void branch_status_set_in(void) { // 识别三岔进入时调用该函数
    assert(branch_status == BRANCH_STATUS_NONE);
    branch_status = BRANCH_STATUS_IN;
    printf(ANSI_COLOR_GREEN "BRANCH_STATUS_NONE -> BRANCH_STATUS_IN" ANSI_COLOR_RESET "\n");
    branch_main();
    extern u8 middle_line[IMG_H];
    for (int i = 0; i < IMG_H; ++i) {
        middle_line[i] = branch_guide[i];
    }
}

void branch_main(void) {
    branch_border_on_diamond_find();

    // 创建边界数组
    if (track_white_id != 0) {
        memcpy(branch_border_left, track_border_left, sizeof branch_border_left);
        memcpy(branch_border_right, track_border_right, sizeof branch_border_right);
        memcpy(branch_guide, track_guide, sizeof branch_guide);
    } else {
        // 保持上一次赛道信息
        return;
    }

    // 入区检测
    if (branch_status == BRANCH_STATUS_NONE) {
        // 由传统寻线实现，识别后调用 branch_status_set_in
    }
    
    if (branch_status == BRANCH_STATUS_IN || branch_status == BRANCH_STATUS_IN_STABLE) {
        // 不需要左拐点
        // {
        //     // 找左拐点
        //     // 跳过丢线行
        //     int i = IMG_H - 1 - 2;
        //     for (; i >= 0; --i) {
        //         if (track_border_left[i] != -1 && track_border_left[i] >= 0 + 3/*左不丢线*/) {
        //             break;
        //         }
        //     }

        //     // 找到最右（上）点作为拐点
        //     int m = -100000;
        //     branch_corner_bottom_left = (pair_t){ -1, -1 };
        //     int cnt = 0; // 向左走的数量
        //     for (; i >= 0; --i) {
        //         if (track_border_left[i] == -1 || track_border_left[i] < 0 + 3/*左丢线*/ 
        //             || branch_border_on_diamond_left[i] - track_border_left[i] > IMG_W * 0.25/*发生跳变*/ && i < IMG_H / 2 /*避开地标的影响*/
        //             || cnt >= 5
        //         ) {
        //             break;
        //         }

        //         if (track_border_left[i] < track_border_left[i + 1]) {
        //             ++cnt;
        //         } else {
        //             cnt = 0;
        //         }

        //         if (track_border_left[i] - i * 0.5 > m) {
        //             m = track_border_left[i] - i * 0.5;
        //             branch_corner_bottom_left.a = i;
        //             branch_corner_bottom_left.b = track_border_left[i];
        //         }
        //     }

        {
            // 找右拐点
            // 跳过丢线行
            int i = IMG_H - 1 - 2;
            for (; i >= 0; --i) {
                if (track_border_right[i] != -1 && track_border_right[i] <= IMG_W - 1 - 3/*右不丢线*/) {
                    break;
                }
            }

            // 找到最左（上）点作为拐点
            int m = 100000;
            branch_corner_bottom_right = (pair_t){ -1, -1 };
            int cnt = 0; // 向右走的数量
            for (; i >= 0; --i) {
                if (track_border_right[i] == -1 || track_border_right[i] > IMG_W - 1 - 3/*右丢线*/ 
                    || track_border_right[i] - branch_border_on_diamond_right[i] > IMG_W * 0.25/*发生跳变*/ && i < IMG_H / 2 /*避开地标的影响*/
                    || cnt >= 5
                ) {
                    break;
                }

                if (track_border_right[i] > track_border_right[i + 1]) {
                    ++cnt;
                } else {
                    cnt = 0;
                }

                if (track_border_right[i] + i * 0.5 < m) {
                    m = track_border_right[i] + i * 0.5;
                    branch_corner_bottom_right.a = i;
                    branch_corner_bottom_right.b = track_border_right[i];
                }
            }

            if (branch_corner_bottom_right.b < IMG_W / 2/*右拐点不会太靠左*/
                || branch_corner_bottom_right.a < IMG_H * 0.2/*下拐点不会太靠上*/
            ) { // 排除菱形上的伪点
                branch_corner_bottom_right = (pair_t){ -1, -1 };
            }
        }

        //     if (branch_corner_bottom_left.b > IMG_W / 2/*左下拐点不会太靠右*/
        //         || branch_corner_bottom_left.a < IMG_H * 0.2/*下拐点不会太靠上*/
        //     ) { // 排除菱形上的伪点
        //         branch_corner_bottom_left = (pair_t){ -1, -1 };
        //     }
        // }

        {
            // 找到菱形上左边界最右的拐点
            int m = -100000;
            branch_corner_in = (pair_t){ -1, -1 };
            for (int i = (branch_status == BRANCH_STATUS_IN_STABLE ? IMG_H - 1 : IMG_H * 0.7); i >= 0; --i) {
                if (branch_border_on_diamond_left[i] != -1 && branch_border_on_diamond_left[i] > m
                    && branch_border_on_diamond_left[i] < IMG_W - 1 - 5/*减少图像边缘上噪点带来的影响*/
                ) {
                    m = branch_border_on_diamond_left[i];
                    branch_corner_in.a = i;
                    branch_corner_in.b = branch_border_on_diamond_left[i];
                }
            }
        }

        if (branch_status == BRANCH_STATUS_IN && branch_corner_in.a != -1 && branch_corner_in.a > IMG_H * 0.4) {
            branch_status = BRANCH_STATUS_IN_STABLE;
            printf(ANSI_COLOR_GREEN "BRANCH_STATUS_IN -> BRANCH_STATUS_IN_STABLE" ANSI_COLOR_RESET "\n");
        } 

        if (branch_corner_in.a == -1/*没这个点*/
            || branch_status == BRANCH_STATUS_IN_STABLE && branch_corner_in.a > IMG_H - 5/*贴底*/
            || branch_status == BRANCH_STATUS_IN_STABLE && branch_corner_in.b > IMG_W - 5 && branch_corner_in.a > IMG_H * 0.6/*贴右且靠下*/
        ) {
            printf("(%d %d)\n", branch_corner_in.a, branch_corner_in.b);
            // return;
            branch_status = BRANCH_STATUS_STAY;
            printf(ANSI_COLOR_GREEN "BRANCH_STATUS_IN -> BRANCH_STATUS_STAY" ANSI_COLOR_RESET "\n");
        } else {
            if (branch_corner_bottom_right.a != -1) {
                // 右边界补线
                my_border_connect(branch_border_right, branch_corner_bottom_right, branch_corner_in);
            } else {
                my_border_connect(branch_border_right, 
                    (pair_t){ IMG_H - 1, IMG_W / 2 + my_track_width[IMG_H - 1] / 2 }, 
                    branch_corner_in
                );
            }
            for (int i = branch_corner_in.a; i >= 0; --i) {
                if (branch_border_on_diamond_left[i] != -1) {
                    branch_border_right[i] = branch_border_on_diamond_left[i];
                } else {
                    assert(branch_border_on_diamond_left[i + 1] != -1);
                    my_border_connect(branch_border_right, 
                        (pair_t){ i + 1, branch_border_on_diamond_left[i + 1] }, 
                        (pair_t){ 0, branch_border_on_diamond_left[i + 1] - (i + 1) }
                    );
                    break;
                }
            }
            branch_guide_update_from_border_right();
        }
    }
    
    if (branch_status == BRANCH_STATUS_STAY) {
        {
            // 找右拐点
            // 跳过丢线行
            int i = IMG_H - 1 - 2;
            for (; i >= 0; --i) {
                if (track_border_right[i] != -1 && track_border_right[i] <= IMG_W - 1 - 3/*右不丢线*/) {
                    break;
                }
            }

            // 找到最左（下）点作为拐点
            int m = 100000;
            branch_corner_top_right = (pair_t){ -1, -1 };
            int cnt = 0; // 向右走的数量
            for (; i >= IMG_H * 0.1; --i) {
                if (track_border_right[i] == -1 || track_border_right[i] > IMG_W - 1 - 3/*右丢线*/ 
                    // || track_border_right[i] - branch_border_on_diamond_right[i] > IMG_W * 0.25/*发生跳变*/ && i < IMG_H / 2 /*避开地标的影响*/
                    // || cnt >= 5
                ) {
                    break;
                }

                if (track_border_right[i] > track_border_right[i + 1]) {
                    ++cnt;
                } else {
                    cnt = 0;
                }

                if (track_border_right[i] - i < m) {
                    m = track_border_right[i] - i;
                    branch_corner_top_right.a = i;
                    branch_corner_top_right.b = track_border_right[i];
                }
            }

            if (branch_corner_top_right.a <= IMG_H * 0.18) { // 排除出三岔后的伪点
                branch_corner_top_right = (pair_t){ -1, -1 };
            }
        }

        if (branch_corner_top_right.a != -1 && branch_corner_top_right.b < IMG_W - 1 - 5/*排除图像边界边缘变黑的噪点干扰*/) {
            branch_status = BRANCH_STATUS_OUT;
            printf(ANSI_COLOR_GREEN "BRANCH_STATUS_STAY -> BRANCH_STATUS_OUT" ANSI_COLOR_RESET "\n");
        } else {
            int i = IMG_H - 1;
            for (; i >= 0; --i) {
                if (branch_border_on_diamond_left[i] == -1) {
                    branch_border_right[i] = IMG_W - 1;
                } else {
                    break;
                }
            }
            for (; i >= 0; --i) {
                if (branch_border_on_diamond_left[i] != -1 && branch_border_on_diamond_left[i] > IMG_W / 2) {
                    branch_border_right[i] = branch_border_on_diamond_left[i];
                } else {
                    branch_border_right[i] = branch_border_right[i + 1] - 1;
                }
            }
            branch_guide_update_from_border_right();
        }
    }
    
    if (branch_status == BRANCH_STATUS_OUT || branch_status == BRANCH_STATUS_OUT_STABLE) {
        pair_t last_branch_corner_top_right = branch_corner_top_right;
        {
            // 找右拐点
            // 跳过丢线行
            int i = IMG_H - 1 - 2;
            for (; i >= 0; --i) {
                if (track_border_right[i] != -1 && track_border_right[i] <= IMG_W - 1 - 3/*右不丢线*/) {
                    break;
                }
            }

            // 找到最左（下）点作为拐点
            int m = 100000;
            branch_corner_top_right = (pair_t){ -1, -1 };
            int cnt = 0; // 向右走的数量
            for (; i >= IMG_H * 0.1; --i) {
                if (track_border_right[i] == -1 || track_border_right[i] > IMG_W - 1 - 3/*右丢线*/ 
                    // || track_border_right[i] - branch_border_on_diamond_right[i] > IMG_W * 0.25/*发生跳变*/ && i < IMG_H / 2 /*避开地标的影响*/
                    // || cnt >= 5
                ) {
                    break;
                }

                if (track_border_right[i] > track_border_right[i + 1]) {
                    ++cnt;
                } else {
                    cnt = 0;
                }

                if (track_border_right[i] - i < m) {
                    m = track_border_right[i] - i;
                    branch_corner_top_right.a = i;
                    branch_corner_top_right.b = track_border_right[i];
                }
            }

            if (branch_corner_top_right.a == IMG_H * 0.1
            ) { // 排除出三岔后的伪点
                branch_corner_top_right = (pair_t){ -1, -1 };
            }
        }

        if (branch_status == BRANCH_STATUS_OUT && branch_corner_top_right.a != -1 && branch_corner_top_right.a > IMG_H * 0.4) {
            branch_status = BRANCH_STATUS_OUT_STABLE;
            printf(ANSI_COLOR_GREEN "BRANCH_STATUS_OUT -> BRANCH_STATUS_OUT_STABLE" ANSI_COLOR_RESET "\n");
        } 

        if (branch_corner_top_right.a != -1 && branch_corner_top_right.a > IMG_H * 0.8
            || branch_status == BRANCH_STATUS_OUT_STABLE && branch_corner_top_right.a == -1
            || branch_status == BRANCH_STATUS_OUT_STABLE && branch_corner_top_right.a < IMG_H * 0.4
            // TODO 再增加积分实现
        ) {
            branch_status = BRANCH_STATUS_NONE;
            printf(ANSI_COLOR_GREEN "BRANCH_STATUS_OUT/BRANCH_STATUS_OUT_STABLE -> BRANCH_STATUS_NONE" ANSI_COLOR_RESET "\n");
        } else {
            if (branch_corner_top_right.a == -1) {
                branch_corner_top_right = last_branch_corner_top_right; // 使用上一次的角点
                assert(last_branch_corner_top_right.a != -1);
            }
            my_border_connect(branch_border_right, 
                (pair_t){ IMG_H - 1, IMG_W / 2 + my_track_width[IMG_H - 1] / 2 }, 
                branch_corner_top_right
            );
            // branch_guide_update_from_border_right();
            branch_guide_update_from_border_right_without_gap();
        }
    }

}