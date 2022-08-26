#include <string.h>
#include <stdio.h>
#include <math.h>
#include "cone.h"
#include "track.h"

uint8_t cone_img[IMG_H][IMG_W];

uint8_t cone_thresholded_img[IMG_H][IMG_W];

// 预处理边界图
// cone_img -> cone_thresholded_img
static void cone_thresholded_find(void) {
    memset(cone_thresholded_img, 0, sizeof cone_thresholded_img);

    /*
        20220605 解决锥桶在图片中小到只有一个像素点的问题
        由于切换到了广角镜头，图像内元素的数量变多，而像素点数量没有变化，故每个锥桶在图像中的像素数量成比例减少
        原先采用的边界检测的方案，边界本身就占用了许多像素点，导致本身不够多的像素数量更少了
        而运动模糊更加剧了这个问题，运动模糊导致边界点检测更加困难，放宽条件后导致边界点更多
        之前有采用锥桶本身与其边界共同构成这个锥桶，但是这个方法一是准确性堪忧，二是复杂，并没有特别的优势
        双通道相减已经尽可能的减少了光照对识别产生的影响
        综上，基于差的边界检测方案已经不符合使用要求，故弃用，现采用更加简单的基于固定阈值的检测方式。
    */
    for (int i = 0; i < IMG_H; ++i) {
        for (int j = 0; j < IMG_W; ++j) {
            if (cone_img[i][j] < 127) {
                cone_thresholded_img[i][j] = 0x00;
            } else {
                cone_thresholded_img[i][j] = 0xff;
            }
        }
    }
}

uint8_t cone_connected_img[IMG_H][IMG_W];
int cone_connected_cnt;
cone_connected_stat_t cone_connected_stat[CONE_CONNECTED_SIZE + 1]; // 多一个为了空出一个给黑色（id=0）

// 找连通块
// cone_thresholded_img -> cone_connected_img
static void cone_connected_find(void) {
    // ref: https://blog.csdn.net/taifyang/article/details/117926802
    // ref: https://www.cnblogs.com/kensporger/p/12463122.html
    memset(cone_connected_img, 0, sizeof cone_connected_img);
    memset(cone_connected_stat, 0, sizeof cone_connected_stat);
#define STACK_SIZE (IMG_H * IMG_W)
    static pair_t stack[STACK_SIZE + 10]; // 预防性多开几个

    cone_connected_cnt = 0;

    for (int i = 0; i < IMG_H; ++i) {
        for (int j = 0; j < IMG_W; ++j) {
            if (cone_connected_img[i][j] == 0
                && cone_thresholded_img[i][j] != 0
            ) {
                cone_connected_img[i][j] = ++cone_connected_cnt;
                assert(cone_connected_cnt <= CONE_CONNECTED_SIZE);
                int stack_top = 0;
                // 后缀++代表：先返回值，后进行+操作；++前缀代表：先进行+操作，后返回值
                stack[stack_top++] = (pair_t){i, j};
                assert(stack_top <= STACK_SIZE);

                // stat start
                cone_connected_stat[cone_connected_cnt].id = cone_connected_cnt;
                cone_connected_stat[cone_connected_cnt].size = 1;
                cone_connected_stat[cone_connected_cnt].color = cone_img[i][j];
                cone_connected_stat[cone_connected_cnt].center = (pair_t){i, j};
                cone_connected_stat[cone_connected_cnt].bottom = (pair_t){i, j};
                int bottom_cnt = 1;
                // stat stop

                while (stack_top) {
                    pair_t p = stack[--stack_top];

                    static const pair_t dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                    for (int k = 0; k < 4; ++k) {
                        pair_t q = (pair_t){ p.a + dirs[k].a, p.b + dirs[k].b };

                        if (0 <= q.a && q.a < IMG_H && 0 <= q.b && q.b < IMG_W
                            && cone_connected_img[q.a][q.b] == 0 // 没有标注过
                            && cone_thresholded_img[q.a][q.b] != 0 // 不是边界
                        ) {
                            cone_connected_img[q.a][q.b] = cone_connected_cnt;
                            stack[stack_top++] = q;
                            assert(stack_top <= STACK_SIZE);

                            // stat start
                            ++cone_connected_stat[cone_connected_cnt].size;
                            cone_connected_stat[cone_connected_cnt].color += cone_img[q.a][q.b];
                            cone_connected_stat[cone_connected_cnt].center.a += q.a;
                            cone_connected_stat[cone_connected_cnt].center.b += q.b;
                            if (q.a > cone_connected_stat[cone_connected_cnt].bottom.a) {
                                cone_connected_stat[cone_connected_cnt].bottom = q;
                                bottom_cnt = 1;
                            } else if (q.a == cone_connected_stat[cone_connected_cnt].bottom.a) { // 求均值以获得一个更加稳定的底端点
                                cone_connected_stat[cone_connected_cnt].bottom.b += q.b;
                                ++bottom_cnt;
                            }
                            // stat stop
                        }
                    }
                }

                // stat start
                cone_connected_stat[cone_connected_cnt].color /= cone_connected_stat[cone_connected_cnt].size;
                cone_connected_stat[cone_connected_cnt].center.a /= cone_connected_stat[cone_connected_cnt].size;
                cone_connected_stat[cone_connected_cnt].center.b /= cone_connected_stat[cone_connected_cnt].size;
                assert(cone_connected_stat[cone_connected_cnt].bottom.a != -1);
                cone_connected_stat[cone_connected_cnt].bottom.b /= bottom_cnt;
                // stat stop
            }
        }
    }
#undef STACK_SIZE
}

cone_connected_stat_t *cone_connected_stat_sorted[CONE_CONNECTED_SIZE + 1];
double cone_connected_stat_sort_value[CONE_CONNECTED_SIZE + 1];
int cone_connected_stat_order[CONE_CONNECTED_SIZE + 1];

static inline bool cmp(cone_connected_stat_t *a, cone_connected_stat_t *b) {
    // 把非cone沉底
    if (!a->cone_flag) return false;
    if (!b->cone_flag) return true;

    return cone_connected_stat_sort_value[a->id] < cone_connected_stat_sort_value[b->id];
}

static void cone_connected_sort(void) {
    // 扇形去扫，按照这个扇形碰到的顺序排序
    if (cone_status != CONE_STATUS_NONE) {
        // 左侧元素，顺时针排序
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            cone_connected_stat_sorted[i] = &cone_connected_stat[i];
            cone_connected_stat_sort_value[i] = (double)(cone_connected_stat[i].bottom.b - (IMG_W / 2 - 1)) / ((IMG_H - 1) - cone_connected_stat[i].bottom.a);
        }
        my_sort((bool (*)(void *, void *))cmp, (void *)cone_connected_stat_sorted, 1, cone_connected_cnt);
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            cone_connected_stat_order[cone_connected_stat_sorted[i]->id] = i;
        }
    } else {
        // assert(false);
    }
}

cone_status_t cone_status;

pair_t cone_entry2;
pair_t cone_exit2;

int cone_border_left[IMG_H]; // 没有边界时为 -1
int cone_border_right[IMG_H]; // 没有边界时为 -1
int cone_guide[IMG_H]; // 没有边界时为 -1

static void cone_guide_update(void) {
    // 计算引导线（中线）
    for (int i = 0; i < IMG_H; ++i) {
        if (cone_border_left[i] != -1 && cone_border_right[i] != -1) {
            cone_guide[i] = (cone_border_left[i] + cone_border_right[i]) / 2;
        } else {
            cone_guide[i] = -1;
        }
    }
}

static void cone_guide_update_from_border_left(void) {
    // 左边界右移来实现引导线
    for (int i = IMG_H - 1; i >= 0; --i) {
        cone_guide[i] = my_min(cone_border_left[i] + my_track_width[i] / 2 * 1.42, IMG_W - 1);
    }
}

static void cone_guide_update_from_border_right(void) {
    // 右边界左移来实现引导线
    for (int i = IMG_H - 1; i >= 0; --i) {
        cone_guide[i] = my_max(cone_border_right[i] - my_track_width[i] / 2 * 1.42, 0);
    }
}

void cone_main(void) {
    // printf("!%d\n", my_real_distance((pair_t){ 35, IMG_W / 2 }, (pair_t){ 40, IMG_W / 2 }));
    // return;
    cone_thresholded_find();
    cone_connected_find();

    // 图像预处理 识别锥桶
    // 忽略小块
    for (int i = 1; i <= cone_connected_cnt; ++i) {
        if (cone_connected_stat[i].size < 3) {
            cone_connected_stat[i].invalid_flag = true;
        }
        if (cone_connected_stat[i].bottom.a > IMG_H / 2
            && cone_connected_stat[i].size < 6
        ) {
            cone_connected_stat[i].invalid_flag = true;
        }
    }

    // 筛选出锥桶
    for (int i = 1; i <= cone_connected_cnt; ++i) {
        cone_connected_stat_t *p = &cone_connected_stat[i];
        if (!p->invalid_flag
            // && p->size <= 400 // 大小适中
            && p->color >= 90 // 足够红
            && (track_white_id == 0
                || track_white_id != 0 && track_border_left[p->center.a] != -1 && p->center.b < track_border_left[p->center.a]
                || track_white_id != 0 && track_border_right[p->center.a] != -1 && p->center.b > track_border_right[p->center.a]
                || track_white_id != 0 && track_border_left[p->center.a] == -1 && track_border_right[p->center.a] == -1
            ) // 锥桶应该在赛道外面，赛道里面的那是标志
        ) {
            p->cone_flag = true;
        }
    }

    // 创建边界数组
    if (track_white_id != 0) {
        memcpy(cone_border_left, track_border_left, sizeof cone_border_left);
        memcpy(cone_border_right, track_border_right, sizeof cone_border_right);
        memcpy(cone_guide, track_guide, sizeof cone_guide);
    } else {
        memset(cone_border_left, 0xff, sizeof cone_border_left);
        memset(cone_border_right, 0xff, sizeof cone_border_right);
        memset(cone_guide, 0xff, sizeof cone_guide);
    }

    // 入区检测
    if (cone_status == CONE_STATUS_NONE) {
        // 判断锥桶是在赛道左面还是右面
        // TODO 测试赛道上多锥桶元素的情况
        int left_cone_cnt = 0, right_cone_cnt = 0;
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            cone_connected_stat_t *p = &cone_connected_stat[i];
            if (p->cone_flag) {
                if (track_white_id != 0 && track_border_left[p->center.a] != -1 && p->center.b < track_border_left[p->center.a]) {
                    left_cone_cnt++;
                }
                if (track_white_id != 0 && track_border_right[p->center.a] != -1 && p->center.b > track_border_right[p->center.a]) {
                    right_cone_cnt++;
                }
            }
        }

        // if (left_cone_cnt + right_cone_cnt == 0) {
        //     printf("cones_at: none\n");
        // } else if (left_cone_cnt > right_cone_cnt) {
        //     printf("cones_at: left\n");
        // } else {
        //     printf("cones_at: right\n");
        // }

        if (left_cone_cnt + right_cone_cnt != 0) {
            // 找到入口的两个锥桶即进入施工区
            // 左侧为例 先找左下面那个，再找右上面那个
            int cone_entry1_id = -1;

            // 找最下锥桶
            int m = -100000;
            for (int i = 1; i <= cone_connected_cnt; ++i) {
                if (cone_connected_stat[i].cone_flag) {
                    if (cone_connected_stat[i].bottom.a > m) {
                        m = cone_connected_stat[i].bottom.a;
                        cone_entry1_id = i;
                    }
                }
            }
            assert(cone_entry1_id != -1);

            if (left_cone_cnt > right_cone_cnt) {
                int cone_entry2_id = -1;

                // 找最右锥桶
                int m = -100000;
                for (int i = 1; i <= cone_connected_cnt; ++i) {
                    if (cone_connected_stat[i].cone_flag) {
                        int a = cone_connected_stat[i].bottom.a;
                        int b = track_border_left[a];
                        int dis = track_white_id != 0 && b != -1 ? my_real_distance(cone_connected_stat[i].bottom, (pair_t){ a, b }) : 10000;
                        // printf("track_cone_dis: %d\n", dis);

                        if (cone_connected_stat[i].bottom.b > m
                            // && cone_connected_stat[i].bottom.a > 10
                            && dis < 15 // 要靠近赛道（根据赛道去调整）
                        ) {
                            m = cone_connected_stat[i].bottom.b;
                            cone_entry2_id = i;
                        }
                    }
                }
                // assert(cone_entry2_id != -1);

                // printf("!%d %d %d\n", cone_entry1_id, cone_entry2_id, cone_connected_stat[cone_entry1_id].bottom.a - cone_connected_stat[cone_entry2_id].bottom.a);
                // printf("!%d", my_real_distance(cone_connected_stat[cone_entry1_id].bottom, cone_connected_stat[cone_entry2_id].bottom));
                int tmp;
                if (cone_entry2_id != -1
                    // && cone_connected_stat[cone_entry1_id].bottom.a - cone_connected_stat[cone_entry2_id].bottom.a >= 25 // 最下最右锥桶行数差 > 25 即代表找到锥桶，可以进入
                    && (tmp = my_real_distance(cone_connected_stat[cone_entry1_id].bottom, cone_connected_stat[cone_entry2_id].bottom)) >= 45 // 两个锥桶真实距离够大
                    // && tmp <= 60 // 在非常远处 tmp 会很大（失真）
                    // && cone_connected_stat[cone_entry2_id].bottom.a > IMG_H * 0.1
                    // && cone_connected_stat[cone_entry1_id].bottom.a > IMG_H * 0.3 // 姿态向右可能会导致概率入锥桶(要求好姿态)
                ) {
                    // cone_status = CONE_STATUS_IN;
                    cone_status = CONE_STATUS_IN_PRE;
                    printf(ANSI_COLOR_RED "CONE_STATUS_NONE -> CONE_STATUS_IN_PRE" ANSI_COLOR_RESET "\n");
                }
            } else {
                // 此处为右侧入锥桶，左右翻转图像处理后再左右翻转打脚行即可
                // 这里不 assert 是因为外界图像干扰可能会导致这里频繁误识别
                // assert(false);
            }
        }
    }

    // 完全入区检测
    if (cone_status == CONE_STATUS_IN_PRE) {
        int cone_entry2_id = -1;

        // 找最右锥桶
        int m = -100000;
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            if (cone_connected_stat[i].cone_flag) {
                int a = cone_connected_stat[i].bottom.a;
                int b = track_border_left[a];
                int dis = track_white_id != 0 && b != -1 ? my_real_distance(cone_connected_stat[i].bottom, (pair_t){ a, b }) : 10000;
                // printf("track_cone_dis: %d\n", dis);

                if (cone_connected_stat[i].bottom.b > m
                    // && cone_connected_stat[i].bottom.a > 10
                    && dis < 15 // 要靠近赛道（根据赛道去调整）
                ) {
                    m = cone_connected_stat[i].bottom.b;
                    cone_entry2_id = i;
                }
            }
        }
        // assert(cone_entry2_id != -1);

        int tmp;
        if (cone_entry2_id != -1
            && cone_connected_stat[cone_entry2_id].bottom.a > IMG_H * 0.15
        ) {
            cone_status = CONE_STATUS_IN;
            printf(ANSI_COLOR_RED "CONE_STATUS_IN_PRE -> CONE_STATUS_IN" ANSI_COLOR_RESET "\n");
        }
    }

    // 对锥桶相对底端中点的角度排序
    cone_connected_sort();

    // 入区
    if (cone_status == CONE_STATUS_IN) {
        int cone_entry2_id = -1;

        // 找靠近赛道且最右面的锥桶 标志锥桶
        int m = -100000;
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            if (cone_connected_stat[i].cone_flag) {
                int a = cone_connected_stat[i].bottom.a;
                int b = track_border_left[a];
                int dis = track_white_id != 0 && b != -1 ? my_real_distance(cone_connected_stat[i].bottom, (pair_t){ a, b }) : 10000;
                // printf("track_cone_dis: %d %d\n", i, dis);
                if (cone_connected_stat[i].bottom.a > m
                    && dis <= 15 // 这个参量需要根据赛道情况具体去调
                ) {
                    m = cone_connected_stat[i].bottom.a;
                    cone_entry2_id = i;
                }
            }
        }
        if (cone_entry2_id == -1
            // || cone_connected_stat[cone_entry2_id].bottom.b > IMG_H * 0.8
            // || track_white_id == 0
            // || track_border_left[IMG_H - 1] == -1
        ) {
            cone_status = CONE_STATUS_IN_AFTER;
            printf(ANSI_COLOR_RED "CONE_STATUS_IN -> CONE_STATUS_IN_AFTER" ANSI_COLOR_RESET "\n");
            // assert(false);
        } else {
            cone_entry2 = cone_connected_stat[cone_entry2_id].bottom;
            // printf("corner2: (%d,%d)\n", cone_entry2.a, cone_entry2.b);

            assert(track_white_id != 0 && track_border_left[IMG_H - 1] != -1);

            // 找最右下锥桶（2a+b）
            int cone_entry1_id = -1;
            int m = -100000;
            for (int i = 1; i <= cone_connected_cnt; ++i) {
                if (cone_connected_stat[i].cone_flag) {
                    if (cone_connected_stat[i].bottom.a * 2 + cone_connected_stat[i].bottom.b > m
                        && i != cone_entry2_id // 不是标志锥桶
                        && cone_connected_stat[i].bottom.a > IMG_H * 0.1 // 在画面下 (1-0.1) 处
                        && cone_connected_stat[i].bottom.b < cone_connected_stat[cone_entry2_id].bottom.b // 在标志锥桶左面
                        && cone_connected_stat[i].bottom.a > cone_connected_stat[cone_entry2_id].bottom.a // 在标志锥桶下面
                    ) {
                        m = cone_connected_stat[i].bottom.a * 2 + cone_connected_stat[i].bottom.b;
                        cone_entry1_id = i;
                    }
                }
            }
            if (cone_entry1_id != -1) {
                pair_t cone_entry1 = cone_connected_stat[cone_entry1_id].bottom;

                // 右边界补线
                my_border_connect(cone_border_right,
                    (pair_t){ my_min(cone_entry1.a + 10, IMG_H - 1), cone_border_right[my_min(cone_entry1.a + 10, IMG_H - 1)] },
                    cone_entry2
                );
                my_border_connect(cone_border_right,
                    cone_entry2,
                    (pair_t){ 0, cone_entry2.b }
                );

                // 左边界补线
                my_border_connect(cone_border_left,
                    (pair_t){ IMG_H - 1, cone_border_left[IMG_H - 1] },
                    (pair_t){ cone_entry1.a, cone_entry1.b }
                );
                my_border_connect(cone_border_left,
                    (pair_t){ cone_entry1.a, cone_entry1.b },
                    (pair_t){ my_max(cone_entry1.a - cone_entry1.b * 3, 0), 0 }
                ); // 斜上补线
                my_border_connect(cone_border_left,
                    (pair_t){ my_max(cone_entry1.a - cone_entry1.b * 3, 0), 0 },
                    (pair_t){ 0, 0 }
                );
            } else {
                // 右边界补线
                my_border_connect(cone_border_right, (pair_t){ IMG_H - 1, cone_border_right[IMG_H - 1] }, cone_entry2);
                my_border_connect(cone_border_right, cone_entry2, (pair_t){ 0, cone_entry2.b });

                // 左边顶头
                my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 }, (pair_t){ 0, 0 });
            }

            cone_guide_update();
        }
    }

    // 二阶段入区，摆正姿态
    if (cone_status == CONE_STATUS_IN_AFTER) {
        // 这里也有可能跳过stay状态进入出状态
        int cone_exit2_id = -1;

        // 找靠近赛道且最右面的锥桶
        int m = -100000;
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            if (cone_connected_stat[i].cone_flag) {
                int a = cone_connected_stat[i].bottom.a;
                int b = track_border_left[a];
                int dis = track_white_id != 0 && b != -1 ? my_real_distance(cone_connected_stat[i].bottom, (pair_t){ a, b }) : 10000;
                // printf("track_cone_dis: %d %d\n", i, dis);
                if (cone_connected_stat[i].bottom.b > m
                    && dis <= 20 // 这个参量需要根据赛道情况具体去调
                ) {
                    m = cone_connected_stat[i].bottom.b;
                    cone_exit2_id = i;
                }
            }
        }
        // printf("cone_exit2_order: %d\n", cone_connected_stat_order[cone_exit2_id]);

        if (cone_exit2_id != -1) {
            cone_status = CONE_STATUS_OUT;
            printf(ANSI_COLOR_RED "CONE_STATUS_IN_AFTER -> CONE_STATUS_OUT" ANSI_COLOR_RESET "\n");
        } else {
            // 左边界锥桶连线
            int last_cone_id = -1;
            for (int i = 1; i <= cone_connected_cnt; ++i) {
                int id = cone_connected_stat_sorted[i]->id;
                if (cone_connected_stat[id].cone_flag) {
                    if (last_cone_id == -1) {
                        my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 }, cone_connected_stat[id].bottom);
                        last_cone_id = id;
                    } else {
                        if (cone_connected_stat[id].bottom.a < cone_connected_stat[last_cone_id].bottom.a) { // 确保不断向前，不会回拐
                            my_border_connect(cone_border_left, cone_connected_stat[last_cone_id].bottom, cone_connected_stat[id].bottom);
                            last_cone_id = id;
                        }
                    }
                }
            }
            assert(last_cone_id != -1); // TODO 解决这个老被 assert 掉的问题
            my_border_connect(cone_border_left, cone_connected_stat[last_cone_id].bottom, (pair_t){ 0, cone_connected_stat[last_cone_id].bottom.b + cone_connected_stat[last_cone_id].bottom.a * 2 });

            // 消除直角，便于打脚拐弯
            // my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 }, (pair_t){ IMG_H / 2, cone_border_left[IMG_H / 2] });
            my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 }, cone_connected_stat[last_cone_id].bottom); // 直接连过去算了

            // 找右侧最下锥桶
            int right_bottom_cone_id = -1;
            for (int i = cone_connected_cnt; i >= 1; --i) {
                int id = cone_connected_stat_sorted[i]->id;
                if (cone_connected_stat[id].cone_flag) {
                    if (cone_connected_stat[id].bottom.a > 60) {
                        right_bottom_cone_id = id;
                    }
                    break;
                }
            }
            // 补齐右边界
            if (right_bottom_cone_id != -1) { // 右侧锥桶可见，将其作为右边界
                my_border_connect(cone_border_right,
                    (pair_t){ IMG_H - 1, cone_connected_stat[right_bottom_cone_id].bottom.b },
                    (pair_t){ 0, cone_connected_stat[right_bottom_cone_id].bottom.b }
                );

                cone_guide_update();
            } else {
                // my_border_connect(cone_border_right, (pair_t){ IMG_H - 1, IMG_W - 1 }, (pair_t){ 0, IMG_W - 1 });

                // cone_guide_update();

                // 右边界丢线严重，使用左边界右移实现中线
                cone_guide_update_from_border_left();
            }
        }
    }

    // 区域里
    if (cone_status == CONE_STATUS_STAY) {
    }

    // 出区
    if (cone_status == CONE_STATUS_OUT) {
        int cone_exit2_id = -1;

        // 找靠近赛道且最上面的锥桶
        int m = 100000;
        for (int i = 1; i <= cone_connected_cnt; ++i) {
            if (cone_connected_stat[i].cone_flag) {
                int a = cone_connected_stat[i].bottom.a;
                int b = track_border_left[a];
                int dis = track_white_id != 0 && b != -1 ? my_real_distance(cone_connected_stat[i].bottom, (pair_t){ a, b }) : 10000;
                // printf("track_cone_dis: %d %d\n", cone_connected_stat_order[i], dis);
                if (cone_connected_stat[i].bottom.a < m
                    && dis <= 25 // 这个参量需要根据赛道情况具体去调
                    && cone_connected_stat[i].bottom.a < IMG_H - 10 // 小车舵机在 10 个周期后才能从向左转到向中（所以下一个周期的识别一定要提早进行）
                ) {
                    m = cone_connected_stat[i].bottom.a;
                    cone_exit2_id = i;
                }
            }
        }
        // printf("cone_exit2_order: %d\n", cone_connected_stat_order[cone_exit2_id]);
        pair_t cone_exit2 = (pair_t){ -1, -1 };
        if (cone_exit2_id == -1) { // 摄像头晃动导致的没有找到点
            // bool found = false;
            if (track_white_id != 0) {
                if (track_connected_stat[track_white_id].size < 1000) {
                    for (int i = IMG_H - 1; i >= 0; --i) {
                        if (track_border_left[i] != -1) {
                            cone_exit2 = (pair_t){ i, track_border_left[i] };
                            // found = true;
                            break;
                        }
                    }
                }
            } else {
                int cone_exit2_id = -1;
                // 放宽条件，找最右面的锥桶
                int m = -100000;
                for (int i = 1; i <= cone_connected_cnt; ++i) {
                    if (cone_connected_stat[i].cone_flag) {
                        if (cone_connected_stat[i].bottom.b > m) {
                            m = cone_connected_stat[i].bottom.b;
                            cone_exit2_id = i;
                        }
                    }
                }
                cone_exit2 = cone_connected_stat[cone_exit2_id].bottom;
            }
            // assert(found); // 没有找到是因为车已经完全走出赛道了
        } else {
            cone_exit2 = cone_connected_stat[cone_exit2_id].bottom;
        }
        if (cone_exit2.a == -1 || cone_exit2.b < IMG_W * 0.4 && cone_exit2.a > IMG_H - 5) {
            cone_status = CONE_STATUS_OUT_AFTER;
            printf(ANSI_COLOR_RED "CONE_STATUS_OUT -> CONE_STATUS_OUT_AFTER" ANSI_COLOR_RESET "\n");
            // assert(false);
        } else {
            // printf("corner_exit2: (%d,%d)\n", cone_exit2.a, cone_exit2.b);

            // 左边界补线
            my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 }, cone_exit2);
            my_border_connect(cone_border_left, cone_exit2, (pair_t){ 0, cone_exit2.b });

            // // 右边界补线
            // my_border_connect(cone_border_right, (pair_t){ IMG_H - 1, IMG_W - 1 }, (pair_t){ 0, IMG_W - 1 }); // 右边顶头

            // cone_guide_update();
            
            // 右边界丢线严重，使用左边界右移实现中线
            cone_guide_update_from_border_left();
        }
    }

    // 二阶段出区，摆正姿态
    if (cone_status == CONE_STATUS_OUT_AFTER) {
        // 寻找是否底端两侧都找到边界了
        bool have_loss_line = false;
        for (int i = IMG_H - 1 - 10; i >= IMG_H - 1 - 15; --i) {
            if (cone_border_left[i] == -1
                || cone_border_left[i] != -1 && cone_border_left[i] < 3
                || cone_border_right[i] == -1
                || cone_border_right[i] != -1 && cone_border_right[i] > IMG_W - 1 - 3
            ) {
                have_loss_line = true;
                break;
            }
        }
        
        // printf("!%d~", cone_border_right[IMG_H - 1] - cone_border_right[IMG_H - 1 - 10]);

        if (!have_loss_line/*左右都有边界（一般为这个）*/
            || cone_border_right[IMG_H - 1] != -1 && cone_border_right[IMG_H - 1 - 10] != -1
            && cone_border_right[IMG_H - 1] < (IMG_W - 5) && cone_border_right[IMG_H - 1 - 10] < (IMG_W - 5)
            && cone_border_right[IMG_H - 1] - cone_border_right[IMG_H - 1 - 10] < 10
            // || mid_point_h == -1/*大直道*/
            // || left_point_h > IMG_W / 2
        ) {
            cone_status = CONE_STATUS_NONE;
            printf(ANSI_COLOR_RED "CONE_STATUS_OUT_AFTER -> CONE_STATUS_NONE" ANSI_COLOR_RESET "\n");
        } else {
            // printf("~%d %d\n", mid_point_h, cone_border_right[mid_point_h]);
            // my_border_connect(cone_border_right, (pair_t){ IMG_H - 1, IMG_W - 1 - 20 }, (pair_t){ IMG_H - 1 - 5, IMG_W - 1 - 20 });
            // my_border_connect(cone_border_right, (pair_t){ IMG_H - 1 - 5, IMG_W - 1 - 20 }, (pair_t){ mid_point_h, cone_border_right[mid_point_h] });
            // // my_border_connect(cone_border_right, (pair_t){ mid_point_h, cone_border_right[mid_point_h] }, (pair_t){ 0, cone_border_right[mid_point_h] }); // 补充边界到最上一行
            // my_border_connect(cone_border_right, (pair_t){ mid_point_h, 0 }, (pair_t){ 0, 0 }); // 斜上 45 补线

            // my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 + 20 }, (pair_t){ IMG_H - 1 - 5, 0 + 20 });
            // my_border_connect(cone_border_left, (pair_t){ IMG_H - 1 - 5, 0 + 20 }, (pair_t){ mid_point_h, cone_border_left[mid_point_h] });
            // // my_border_connect(cone_border_left, (pair_t){ mid_point_h, cone_border_left[mid_point_h] }, (pair_t){ 0, cone_border_left[mid_point_h] }); // 补充边界到最上一行
            // my_border_connect(cone_border_left, (pair_t){ mid_point_h, 0 }, (pair_t){ 0, 0 }); // 斜上 45 补线

            // 找右边界最左面的点
            int left_point_h = -1;
            int m = 100000;
            for (int i = IMG_H - 1 - 5; i >= IMG_H * 0.1; --i) {
                if (cone_border_right[i] != -1 && cone_border_right[i] <= m) {
                    m = cone_border_right[i];
                    left_point_h = i;
                }
            }

            my_border_connect(cone_border_right, (pair_t){ IMG_H - 1, IMG_W / 2 + my_track_width[IMG_H - 1] / 2 }, (pair_t){ IMG_H - 1 - 5, IMG_W / 2 + my_track_width[IMG_H - 1 - 5] / 2 });
            my_border_connect(cone_border_right, (pair_t){ IMG_H - 1 - 5, IMG_W / 2 + my_track_width[IMG_H - 1 - 5] / 2 }, (pair_t){ left_point_h, 0 });
            my_border_connect(cone_border_right, (pair_t){ left_point_h, 0 }, (pair_t){ 0, 0 });
            
            my_border_connect(cone_border_left, (pair_t){ IMG_H - 1, 0 }, (pair_t){ 0, 0 });

            cone_guide_update();
            
            // 左边界丢线严重，使用右边界左移实现中线
            // cone_guide_update_from_border_right();
        }
    }

    // if (cone_status != CONE_STATUS_NONE) {
    //     // 补全前几行没有赛道时候的 guide 值
    //     int last_guide_h = -1;
    //     for (int i = IMG_H - 1; i >= 0; --i) {
    //         if (cone_guide[i] != -1) {
    //             last_guide_h = i;
    //         } else {
    //             break;
    //         }
    //     }
    //     assert(last_guide_h != -1);
    //     for (int i = last_guide_h - 1; i >= 0; --i) {
    //         cone_guide[i] = cone_guide[last_guide_h];
    //     }
    // }
}