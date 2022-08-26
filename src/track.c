#include <string.h>
#include <stdio.h>
// #include <time.h>
#include "track.h"

uint8_t track_img[IMG_H][IMG_W];

uint8_t track_not_border_img[IMG_H][IMG_W];

// 预处理边界图
// track_img -> track_not_border_img
static void track_not_border_find(void) {
    // blur start
    // 方案1:节省空间,但是可能无法循环展开(两个方案实际等价)
    for (int i = 0; i <= IMG_H - 1 - 1; ++i) {
        for (int j = 0; j <= IMG_W - 1 - 1; ++j) {
            track_img[i][j] = ((uint16_t)track_img[i][j] + track_img[i][j + 1] + track_img[i + 1][j] + track_img[i + 1][j + 1]) >> 2;
        }
    }
    for (int i = 0; i <= IMG_H - 1; ++i) {
        track_img[i][IMG_W - 1] = track_img[i][IMG_W - 1 - 1];
    }
    for (int j = 0; j <= IMG_W - 1; ++j) {
        track_img[IMG_H - 1][j] = track_img[IMG_H - 1 - 1][j];
    }
    
    // 方案2:不节省空间,但是可以循环展开
    // static uint8_t track_img_blured[IMG_H][IMG_W];
    // for (int i = 0; i <= IMG_H - 1 - 1; ++i) {
    //     for (int j = 0; j <= IMG_W - 1 - 1; ++j) {
    //         track_img_blured[i][j] = ((uint16_t)track_img[i][j] + track_img[i][j + 1] + track_img[i + 1][j] + track_img[i + 1][j + 1]) >> 2;
    //     }
    // }
    // for (int i = 0; i <= IMG_H - 1; ++i) {
    //     track_img_blured[i][IMG_W - 1] = track_img[i][IMG_W - 1 - 1];
    // }
    // for (int j = 0; j <= IMG_W - 1; ++j) {
    //     track_img_blured[IMG_H - 1][j] = track_img[IMG_H - 1 - 1][j];
    // }
    // memcpy(track_img, track_img_blured, sizeof track_img_blured);
    // blur end

    memset(track_not_border_img, 0, sizeof track_not_border_img);

    for (int i = 0; i < IMG_H; ++i) {
        for (int j = 0; j < IMG_W; ++j) {
            // 这里逻辑变动时需要同步更改 branch.c 中 branch_border_on_diamond_find 函数
            int a1 = i - TRACK_BORDER_R, a2 = i + TRACK_BORDER_R,
                b1 = j - TRACK_BORDER_R, b2 = j + TRACK_BORDER_R;
            if (0 <= a1 && a1 < IMG_H && 0 <= a2 && a2 < IMG_H
                && 0 <= b1 && b1 < IMG_W && 0 <= b2 && b2 < IMG_W
                && my_max(
                    my_abs((int)track_img[a1][j] - track_img[a2][j]),
                    my_abs((int)track_img[i][b1] - track_img[i][b2])
                ) < TRACK_BORDER_DIFF
            ) {
                track_not_border_img[i][j] = 0xFF;
            }
        }
    }
}

uint8_t track_connected_img[IMG_H][IMG_W]; // 有效（非黑色（边界））id从1开始标注
int track_connected_cnt;
track_connected_stat_t track_connected_stat[TRACK_CONNECTED_SIZE + 1]; // 多一个为了空出一个给黑色（边界）（id=0）

// 找连通块
// track_not_border_img -> track_connected_img
static void track_connected_find(void) {
    // ref: https://blog.csdn.net/taifyang/article/details/117926802
    // ref: https://www.cnblogs.com/kensporger/p/12463122.html
    memset(track_connected_img, 0, sizeof track_connected_img);
    memset(track_connected_stat, 0, sizeof track_connected_stat);
#define STACK_SIZE (IMG_H * IMG_W)
    static pair_t stack[STACK_SIZE + 10]; // 预防性多开几个

    track_connected_cnt = 0;
    for (int i = 0; i < IMG_H; ++i) {
        for (int j = 0; j < IMG_W; ++j) {
            if (track_connected_img[i][j] == 0
                && track_not_border_img[i][j] != 0
            ) {
                track_connected_img[i][j] = ++track_connected_cnt;
                assert(track_connected_cnt <= TRACK_CONNECTED_SIZE);
                int stack_top = 0;
                // 后缀++代表：先返回值，后进行+操作；++前缀代表：先进行+操作，后返回值
                stack[stack_top++] = (pair_t){i, j};
                assert(stack_top <= STACK_SIZE);

                while (stack_top) {
                    pair_t p = stack[--stack_top];

                    // stat start
                    ++track_connected_stat[track_connected_cnt].size;
                    track_connected_stat[track_connected_cnt].color += track_img[p.a][p.b];
                    track_connected_stat[track_connected_cnt].center.a += p.a;
                    track_connected_stat[track_connected_cnt].center.b += p.b;
                    // stat stop

                    static const pair_t dirs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                    for (int k = 0; k < 4; ++k) {
                        pair_t q = (pair_t){ p.a + dirs[k].a, p.b + dirs[k].b };

                        if (0 <= q.a && q.a < IMG_H && 0 <= q.b && q.b < IMG_W
                            && track_connected_img[q.a][q.b] == 0
                            && track_not_border_img[q.a][q.b] != 0
                        ) {
                            track_connected_img[q.a][q.b] = track_connected_cnt;
                            stack[stack_top++] = q;
                            assert(stack_top <= STACK_SIZE);
                        }
                    }
                }

                // stat start
                track_connected_stat[track_connected_cnt].id = track_connected_cnt;
                track_connected_stat[track_connected_cnt].color /= track_connected_stat[track_connected_cnt].size;
                track_connected_stat[track_connected_cnt].center.a /= track_connected_stat[track_connected_cnt].size;
                track_connected_stat[track_connected_cnt].center.b /= track_connected_stat[track_connected_cnt].size;
                // stat stop
            }
        }
    }
#undef STACK_SIZE
}

track_connected_stat_t *track_connected_stat_sorted[TRACK_CONNECTED_SIZE + 1];

// track_connected_stat_sorted 按照 size 键从大到小排序
static inline bool cmp(track_connected_stat_t *a, track_connected_stat_t *b) {
    return a->size > b->size;
}

// 对连通块大小从大到小排序
static void track_connected_sort(void) {
    // 忽略小块
    for (int i = 1; i <= track_connected_cnt; ++i) {
        if (track_connected_stat[i].size < 10) {
            track_connected_stat[i].invalid_flag = true;
        }
    }

    for (int i = 1; i <= track_connected_cnt; ++i) {
        track_connected_stat_sorted[i] = &track_connected_stat[i];
    }
    my_sort((bool (*)(void *, void *))cmp, (void *)track_connected_stat_sorted, 1, track_connected_cnt);
}

int track_white_id;

static void track_white_find(void) {
    // 找又大又白的赛道块
    track_white_id = 0;
    for (int i = 1; i <= track_connected_cnt; ++i) {
        if (!track_connected_stat_sorted[i]->invalid_flag
            && track_connected_stat_sorted[i]->size >= 150 // 大
            && track_connected_stat_sorted[i]->color >= 160 // 白 // TODO 150?
        ) {
            track_connected_stat_sorted[i]->track_flag = true;
            track_white_id = track_connected_stat_sorted[i]->id;
            break; // 第一个
        }
    }
}

pair_t track_border[TRACK_BORDER_SIZE]; // 存储边界信息
int track_border_n;

track_border_stat_t track_border_stat[TRACK_BORDER_SIZE];

int track_border_bottom_id, track_border_top_id; // 存储封底和封顶点

// 八邻域边缘跟踪算法
// ref: https://blog.csdn.net/sinat_31425585/article/details/78558849
static void track_border_find(void) {
    if (track_white_id == 0) { // 没找到赛道
        return;
    }

    // 从画布（外面）从下往上向中心点搜索找到的第一个边界点即为种子点
    // 这个方法一定能找到一条边界且这个边界是外边界
    // 证明：这个图形是连通并且闭合的，如果画一条线并没有这个图形相交，则这个图形一定是在这条线的左边或者右边，
    // 那么这个图形的中心点一定在这条线的左面或者右面，与假设矛盾，得证
    int j = track_connected_stat[track_white_id].center.b;
    int i;
    for (i = IMG_H - 1; i >= 0; --i) {
        if (track_connected_img[i][j] == track_white_id) { // 找到的第一个非边界白色赛道
            break;
        }
    }
    assert(i >= 0); // 没有在遍历所有后仍然没有找到
    i += 1; // 回退一格到边界上

    // 该数组的顺序与正确的八邻域实现有关，不要随意调换里面元素顺序
    // static pair_t dirs[8] = {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}}; // 向右扫用
    static const pair_t dirs[8] = {{-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}}; // 向左扫用 逆时针去贴边界 最后的边界是顺时针标记的
    // 1 0 7
    // 2   6
    // 3 4 5

    pair_t p = {i, j}; // 种子点
    track_border_n = 0;
    track_border[track_border_n++] = p;
    // invalid_flag 在最后填充

    // 封顶封底
    track_border_bottom_id = 0;
    track_border_top_id = 0;

    int dir = 0; // 边界扫描开始点（根据边界方向所得） // 向上扫 // 上面那个一定是非边界，那么逆时针旋转，指针就能贴到左侧边界上
    bool track_border_closed = false; // 曲线闭合
    while (track_border_n < TRACK_BORDER_SIZE) {
        bool notfound_flag = true;
        for (int i = 0; i < 8; ++i) {
            int cur_dir = dir + i;
            if (cur_dir >= 8)
                cur_dir -= 8;
            pair_t q = {p.a + dirs[cur_dir].a, p.b + dirs[cur_dir].b};
            if (0 <= q.b && q.b < IMG_W) { // 边界外
                // 看是否为边界点，边界点也设置值
                // ps. 边界线不影响寻线，算法寻线过程中会始终保持逆时针旋转（由 dir 决定）
                // 边界线，或者边界线形成分支时，该算法会始终选择最”里“的分支
                // 之前有想过使用扫描顺序，先按照原方向扫描，然后扫左右的
                // 也有想过八邻域中找最大颜色的
                // 但是这两种扫描会导致在宽边界中转圈，加 vst 数组并没有作用
                int a1 = q.a - TRACK_BORDER_R, a2 = q.a + TRACK_BORDER_R,
                    b1 = q.b - TRACK_BORDER_R, b2 = q.b + TRACK_BORDER_R;
                bool invalid_flag = !(0 <= a1 && a1 < IMG_H && 0 <= a2 && a2 < IMG_H
                    && 0 <= b1 && b1 < IMG_W && 0 <= b2 && b2 < IMG_W); // 在边界

                if (track_not_border_img[q.a][q.b] == 0x00) {
                    p = q;

                    if (p.a == track_border[0].a && p.b == track_border[0].b) { // 成环了
                        track_border_stat[0].invalid_flag = invalid_flag; // 填充第一个

                        track_border_closed = true;
                        // notfound_flag = true
                    } else {
                        // 封顶封底
                        if (p.a > track_border[track_border_bottom_id].a) {
                            track_border_bottom_id = track_border_n;
                        }
                        if (p.a < track_border[track_border_top_id].a) {
                            track_border_top_id = track_border_n;
                        }

                        track_border_stat[track_border_n].invalid_flag = invalid_flag;
                        track_border[track_border_n++] = p;

                        dir = cur_dir - 2;
                        if (dir < 0)
                            dir += 8;

                        notfound_flag = false;
                    }
                    break;
                }
            }
        }
        if (notfound_flag)
            break;
    }
    assert(track_border_closed);
}

int track_border_left[IMG_H]; // 没有边界时为 -1 在 track_white_id !=0 时才有效，使用前需要先查该数组
int track_border_right[IMG_H]; // 没有边界时为 -1 在 track_white_id !=0 时才有效，使用前需要先查该数组
int track_guide[IMG_H]; // 没有边界时为 -1 在 track_white_id !=0 时才有效，使用前需要先查该数组

static void track_guide_find(void) {
    if (track_white_id == 0) { // 没找到赛道
        // 保持上次赛道信息
        return;
    }

    memset(track_border_left, 0xff, sizeof track_border_left);
    memset(track_border_right, 0xff, sizeof track_border_right);
    memset(track_guide, 0xff, sizeof track_guide);

    assert(track_border_n != 1); // 否则会出现死循环

    // 扫左边界
    // for (int i = track_border_bottom_id; i != track_border_top_id + 1; ++i, (i = i == track_border_n ? 0 : i)) {
    for (
        int i = track_border_bottom_id;
        i != ((track_border_top_id + 1) == track_border_n ? 0 : (track_border_top_id + 1));
        ++i, (i = i == track_border_n ? 0 : i)
    ) {
        if (track_border_left[track_border[i].a] == -1
            || track_border[i].b < track_border_left[track_border[i].a]
        ) {
            track_border_left[track_border[i].a] = track_border[i].b;
            // TODO 统计边界丢线（边界点为图像边界）
        }
    }

    // 扫右边界
    // for (int i = track_border_bottom_id; i != track_border_top_id - 1; (i = i == 0 ? track_border_n : i), --i) {
    for (
        int i = track_border_bottom_id; 
        i != (track_border_top_id == 0 ? track_border_n : track_border_top_id) - 1; 
        (i = i == 0 ? track_border_n : i), --i
    ) {
        if (track_border_right[track_border[i].a] == -1
            || track_border[i].b > track_border_right[track_border[i].a]
        ) {
            track_border_right[track_border[i].a] = track_border[i].b;
        }
    }

    // 计算引导线（中线）
    for (int i = 0; i < IMG_H; ++i) {
        if (track_border_left[i] != -1 && track_border_right[i] != -1) {
            track_guide[i] = (track_border_left[i] + track_border_right[i]) / 2;
        }
    }
}

void track_main(void) { // 6ms O3优化后：1ms
    // // 时间测量
    // clock_t start = clock();
    track_not_border_find(); // 1.5ms O3优化后：0.1ms
    // clock_t t1 = clock();
    track_connected_find(); // 4.4ms O3优化后：0.8ms
    // clock_t t2 = clock();
    track_connected_sort();
    track_white_find();
    track_border_find();
    track_guide_find();
    // clock_t t6 = clock();
    // // 时间测量
    // printf("%d\t%d\t%d\t\n", 
    //     (int)((t1 - start) * 10000 / CLOCKS_PER_SEC),
    //     (int)((t2 - t1) * 10000 / CLOCKS_PER_SEC),
    //     (int)((t6 - start) * 10000 / CLOCKS_PER_SEC)
    // );
}