#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

typedef struct {
    int a, b;
} pair_t;

static inline int my_abs(int a) { return a < 0 ? -a : a; }
static inline int my_max(int a, int b) { return a > b ? a : b; }
static inline int my_min(int a, int b) { return a < b ? a : b; }

void my_sort(bool (*cmp)(void *, void *), void *a[], int l, int r);

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// 数据类型声明
typedef char i8;       //  8 bits
typedef short int i16; // 16 bits
typedef long int i32;  // 32 bits
typedef long long i64; // 64 bits

typedef unsigned char u8;       //  8 bits
typedef unsigned short int u16; // 16 bits
typedef unsigned long int u32;  // 32 bits
typedef unsigned long long u64; // 64 bits

#define IMG_H 300 // TODO 根据自己小车的摄像头角度位置填充
#define IMG_W 400 

static double my_perspective_m[3][3] = {
    0, 0, 0,
    0, 0, 0,
    0, 0, 0,
    // TODO 根据自己小车的摄像头角度位置填充
};

// 逆透视变换
static inline pair_t my_perspective_transform(pair_t in) {
    // Ref: https://docs.opencv.org/4.x/d2/de8/group__core__array.html#gad327659ac03e5fd6894b90025e6900a7
    
    // double a[1][3] = { in.b, in.a, 1 }; // x, y, 1
    // double b[1][3] = {};
    // for (int i = 0; i < 1; ++i) {
    //     for (int j = 0; j < 3; ++j) {
    //         for (int k = 0; k < 3; ++k) {
    //             b[i][j] += a[i][k] * my_perspective_m[j][k]; // 与标准矩阵相比，my_perspective_m矩阵需要转置一下再参与运算
    //         }
    //     }
    // }
    // b[0][0] /= b[0][2];
    // b[0][1] /= b[0][2];
    // b[0][2] /= b[0][2];
    // std::cout << b[0][0] << " " << b[0][1] << " " << b[0][2] << " " << std::endl;
    // pair_t out = { b[0][1], b[0][0] };

    double x1 = in.b, y1 = in.a, w1 = 1;
    double x2 = x1 * my_perspective_m[0][0] + y1 * my_perspective_m[0][1] + w1 * my_perspective_m[0][2], 
        y2 = x1 * my_perspective_m[1][0] + y1 * my_perspective_m[1][1] + w1 * my_perspective_m[1][2], 
        w2 = x1 * my_perspective_m[2][0] + y1 * my_perspective_m[2][1] + w1 * my_perspective_m[2][2];
    pair_t out = { (int)(y2 / w2), (int)(x2 / w2) };
    return out;
}

// 计算经过逆透视变换后的真实距离
// 这个函数很耗时间，尽量不要使用
static inline int my_real_distance(pair_t in1, pair_t in2) {
    pair_t out1 = my_perspective_transform(in1);
    pair_t out2 = my_perspective_transform(in2);
    // printf("%d %d %d %d\n", out1.a, out1.b, out2.a, out2.b);
    return sqrt((out1.a - out2.a) * (out1.a - out2.a) + (out1.b - out2.b) * (out1.b - out2.b));
}

// 求绝对角度（相对于 x 轴），类 arctan 函数
// return: absolute angle [-90, 270)
static inline int my_angle(pair_t p1, pair_t p2) {
    int rev = 0;
    if (p1.b > p2.b) { // angle 在 [90, 270) 先映射到 [-90, 90)
        pair_t t = p1; p1 = p2; p2 = t;
        rev = 1;
    }
    int dy = p2.a - p1.a;
    int dx = p2.b - p1.b;
    int s = 1;
    if (dy < 0) {
        dy = -dy;
        s = -1;
    }
    int rst = 0;
    if (dx == 0 && dy == 0) return 0; // 零向量给个默认值 防止下方除零错误
    if (dy < dx) { // 用两条直线近似 arctan
        rst = (int)45 * dy / dx;
    } else {
        rst = 90 - (int)45 * dx / dy;
    }
    if (s == -1) rst = -rst;
    if (rev == 1) rst += 180;
    return rst;
}

// 求边界上的角度差和角度突出来的那个方向
// return: reletive angle [0, 180)
// return: dir [0, 360)
static inline pair_t my_diff_angle(pair_t pl, pair_t p, pair_t pr) {
    int a1 = my_angle(pl, p),
        a2 = my_angle(p, pr);
    int adiff = a2 - a1;
    if (adiff < 0) adiff += 360; // 负数变正
    int dir = a1 + (adiff >> 1) - 90; // 两射线角中线的反方向（即突出来的方向） // 具体在纸上画一下就出来了qwq
    if (dir < 0) dir += 360;
    if (adiff > 180) adiff = 360 - adiff; // 用较小一侧的角
    pair_t rst = { adiff, dir };
    return rst;
}

static inline void my_border_connect(int border[IMG_H], pair_t p1, pair_t p2) {
    if (p1.a == p2.a) {
        border[p1.a] = (p1.b + p2.b) / 2;
    } else {
        if (p1.a > p2.a) {
            pair_t t = p1; p1 = p2; p2 = t;
        }
        // 现已支持负数坐标补线
        for (int i = my_max(p1.a, 0); i <= my_min(p2.a, IMG_H - 1); ++i) {
            border[i] = my_min(my_max(((p2.a - i) * p1.b + (i - p1.a) * p2.b) / (p2.a - p1.a), 0), IMG_W - 1);
        }
    }
}

// 标准赛道宽度 45cm
// my_track_width[i] 代表图像中第i行的白色赛道像素点个数
static int my_track_width[IMG_H] = {
    0
    // TODO 根据自己小车的摄像头角度位置填充
};

#endif