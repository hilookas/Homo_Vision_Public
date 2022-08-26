#include "main.hpp"

#define IMG_DEBUGING // 显示允许
// #define IMG_DEBUGING_LIVE_WATCH
#define CONE_DEBUGING // 锥桶调试部分
#define COMM_ENABLED
// #define COMM_TIMING
// #define VOFA_ENABLED
// #define MAIN_TIMING
#define BRANCH_DEBUGING

#include <iostream>
#include <fstream>
#include <ctime>
#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

extern "C" {
#include "cap.h"
#include "track.h"
#include "cone.h"
#include "branch.h"
#include "utils.h"
#ifdef COMM_ENABLED
#include "comm.h"
#endif
#ifdef VOFA_ENABLED
#include "vofa.h"
#endif
}

extern "C" {

// 调试显示函数
void live_watch(const char *name, int val) {
#ifdef IMG_DEBUGING_LIVE_WATCH
    printf("%s: %04d  ", name, val);
#endif
}

u8 img[IMG_H][IMG_W];
u8 middle_line[IMG_H];
int control_line = 35;

u8 go_flag = 0;
u8 gogo_flag = 0;
u8 element = 0;
u8 garage_stop_cnt = 0;

void timer() { // 中断回调函数
    if (element == 2) {
        garage_stop_cnt++;
    }
}
}

#define CV_COLOR_RED cv::Scalar(0, 0, 255)   //纯红
#define CV_COLOR_GREEN cv::Scalar(0, 255, 0) //纯绿
#define CV_COLOR_BLUE cv::Scalar(255, 0, 0)  //纯蓝

#define CV_COLOR_DARKGRAY cv::Scalar(169, 169, 169) //深灰色
#define CV_COLOR_DARKRED cv::Scalar(0, 0, 139)      //深红色
#define CV_COLOR_ORANGERED cv::Scalar(0, 69, 255)   //橙红色

#define CV_COLOR_CHOCOLATE cv::Scalar(30, 105, 210) //巧克力
#define CV_COLOR_GOLD cv::Scalar(10, 215, 255)      //金色
#define CV_COLOR_YELLOW cv::Scalar(0, 255, 255)     //纯黄色

#define CV_COLOR_OLIVE cv::Scalar(0, 128, 128)        //橄榄色
#define CV_COLOR_LIGHTGREEN cv::Scalar(144, 238, 144) //浅绿色
#define CV_COLOR_DARKCYAN cv::Scalar(139, 139, 0)     //深青色

#define CV_COLOR_SKYBLUE cv::Scalar(230, 216, 173) //天蓝色
#define CV_COLOR_INDIGO cv::Scalar(130, 0, 75)     //藏青色
#define CV_COLOR_PURPLE cv::Scalar(128, 0, 128)    //紫色

#define CV_COLOR_PINK cv::Scalar(203, 192, 255)    //粉色
#define CV_COLOR_DEEPPINK cv::Scalar(147, 20, 255) //深粉色
#define CV_COLOR_VIOLET cv::Scalar(238, 130, 238)  //紫罗兰

#define CV_COLOR_BLACK cv::Scalar(0, 0, 0)  //黑色

void draw_line(cv::Mat frame, u8 start_x, u8 start_y, u8 end_x, u8 end_y, cv::Scalar CV_COLOR) {
    cv::Point start = cv::Point(start_x, start_y); //直线起点
    cv::Point end = cv::Point(end_x, end_y);       //直线终点
    cv::line(frame, start, end, CV_COLOR);
}

void draw_point(cv::Mat frame, u8 x, u8 y, cv::Scalar CV_COLOR) {
    cv::Point point = cv::Point(x, y);
    cv::circle(frame, point, 1, CV_COLOR);
}

// 以中心点坐标和半径画圆
void draw_circle(cv::Mat frame, u8 x, u8 y, u8 r, cv::Scalar CV_COLOR) {
    cv::Point point = cv::Point(x, y);
    cv::circle(frame, point, r, cv::Scalar(0, 0, 255));
}

void draw_str(cv::Mat frame, u8 x, u8 y, const std::string &str) {
    cv::Point origin = cv::Point(x, y);
    cv::putText(frame, str, origin, cv::FONT_HERSHEY_SIMPLEX, 0.4, CV_RGB(255, 230, 0));
}

void draw_num(cv::Mat frame, u8 x, u8 y, int num) {
    cv::Point origin = cv::Point(x, y);
    cv::putText(frame, std::to_string(num), origin, cv::FONT_HERSHEY_SIMPLEX, 0.4, CV_RGB(0, 255, 0));
}

// CONE_DEBUGING START

const int show_scale = 2;
int show_pos = -1;

// my imshow
void my_image_show(const char *name, const cv::Mat &mat, int show_scale_ = show_scale) {
    // cv::namedWindow(name, cv::WINDOW_NORMAL);
    cv::Mat buf;
    if (strcmp(name, "perspective") == 0) {
        cv::resize(mat, buf, cv::Size(mat.cols * show_scale / 2, mat.rows * show_scale / 2), 0, 0, cv::INTER_NEAREST);
    } else {
        cv::resize(mat, buf, cv::Size(mat.cols * show_scale, mat.rows * show_scale), 0, 0, cv::INTER_NEAREST);
    }
    cv::imshow(name, buf);
    cv::setWindowTitle(name, show_pos == -1 ? cv::format("%s: live\n", name) : cv::format("%s: %d\n", name, show_pos));
}

extern "C" {
// my imshow (c style image)
void my_img_show(const char *name, uint8_t img[IMG_H][IMG_W]) {
    cv::Mat image(IMG_H, IMG_W, CV_8UC1, img);
    my_image_show(name, image);
}
}

typedef struct {
    const char *name;
    const char *pers_name;
    cv::Mat image;
    cv::Mat pers_image;
    cv::RNG rng;
    pair_t last_p;
    pair_t last_pers_p;
} my_mouse_data_t;
static my_mouse_data_t my_mouse_data;

// 画布上鼠标操作处理函数
// event鼠标事件代号，x,y鼠标坐标，flags拖拽和键盘操作的代号
static void on_mouse(int event, int x, int y, int flags, void *raw_data) {
    my_mouse_data_t *data = (my_mouse_data_t *)raw_data;
    switch (event) {
    case cv::EVENT_LBUTTONDOWN: // 按下左键
        pair_t p = { y / show_scale, x / show_scale };
        int len = sqrt(pow(data->last_p.a - p.a, 2) + pow(data->last_p.b - p.b, 2));

        pair_t pers_p = my_perspective_transform(p);
        int pers_len = my_real_distance(data->last_pers_p, pers_p);

        std::cout << cv::format("(%d,%d) %d (%d,%d) %d\n", p.a, p.b, len, pers_p.a, pers_p.b, pers_len);

        cv::Scalar color(data->rng.uniform(0, 255), data->rng.uniform(0, 255), data->rng.uniform(0, 255));

        cv::Point p_cv(p.b, p.a);
        cv::putText(data->image, cv::format("(%d,%d)", p.a, p.b), p_cv, cv::FONT_HERSHEY_SIMPLEX, 0.25, color);
        cv::circle(data->image, p_cv, 1, color, 2, 8, 0);
        my_image_show(data->name, data->image); // 刷新图片

        cv::Point pers_p_cv(pers_p.b, pers_p.a);
        cv::putText(data->pers_image, cv::format("(%d,%d)", pers_p.a, pers_p.b), pers_p_cv, cv::FONT_HERSHEY_SIMPLEX, 0.25, color);
        cv::circle(data->pers_image, pers_p_cv, 1, color, 2, 8, 0);
        my_image_show(data->pers_name, data->pers_image); // 刷新图片

        data->last_p = p;
        data->last_pers_p = pers_p;
        break;
    }
}

// CONE_DEBUGING END

inline int calc_time_diff(struct timespec &time1, struct timespec &time0) {
    return (time1.tv_sec - time0.tv_sec) * 1000000 + (time1.tv_nsec - time0.tv_nsec) / 1000;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

    // convert_record_from_splitted_file();
    // return 0;

    // usage: ./main {mode} {arg2}
    int mode = 0;
    // mode == 0 正常跑
    // mode == 1 正常跑+录制
    // mode == 2 回放模式+显示
    // mode == 3 正常跑+显示
    if (argc >= 2) {
        sscanf(argv[1], "%d", &mode);
    }

    bool record_enabled = false, replay_enabled = false, debug_enabled = false, meta_enabled = true;
    if (mode == 0) {
    } else if (mode == 1) {
        record_enabled = true;
    } else if (mode == 2) {
        replay_enabled = true;
        debug_enabled = true;
    } else if (mode == 3) {
        debug_enabled = true;
    } else {
        assert(false);
    }

    // 增加快进功能
    int replay_fast_forward = -1;
    if (mode == 2 && argc >= 3) { // ./main 2 124
        sscanf(argv[2], "%d", &replay_fast_forward);
    }
    if (mode == 2 && argc >= 4) { // ./main 2 124 0
        int tmp;
        sscanf(argv[3], "%d", &tmp);
        meta_enabled = tmp;
    }
    if (replay_fast_forward != -1) {
        debug_enabled = false;
        printf("Fast forward (with calc) to #%d...\n", replay_fast_forward);
    }

    // 初始化摄像头（新）
    bool ret = cap_init(replay_enabled, record_enabled, meta_enabled);
    assert(!ret);

#ifdef COMM_ENABLED
    if (!replay_enabled) {
        // 初始化串口通信
        ret = comm_init();
        assert(!ret);
    }
#endif

#ifdef COMM_TIMING
    // 测试串口来回延迟
    while (true) {
        clock_t start = clock();
        printf("send\n");
        uint8_t payload[10] = { 0x01 }; // 数据
        bool ret = comm_send_blocking(COMM_TYPE_PING, payload);
        assert(!ret);
        while (true) {
            // 串口通讯数据接收（从 TC264 端）
            // TODO 对串口数据接收过程计时，评估对性能的影响
            comm_type_t type;
            uint8_t payload[10]; // 数据

            // 尽量清空读缓冲区（同步两台设备的周期），并且返回最后一组数据
            bool last_ret = true; // 默认为没有数据（发生错误）
            bool ret = comm_recv_poll(&type, payload);
            while (!ret) {
                last_ret = ret;
                ret = comm_recv_poll(&type, payload);
            }
            ret = last_ret;

            if (!ret) { // success
                if (type == COMM_TYPE_PONG) { // 串口测试
                    // pong back
                    printf("recv %02x\n", payload[0]);
                    break;
                }
            }
        }
        printf("%dms\n", (int)((clock() - start) * 1000 / CLOCKS_PER_SEC));
    }
#endif

#ifdef VOFA_ENABLED
    if (debug_enabled) {
        // vofa 连接
        ret = vofa_init();
        assert(!ret);
    }
#endif

    // 定时器部分
    auto timer_start = std::chrono::steady_clock::now();

    while (true) {
#ifdef MAIN_TIMING
        // 时间测量
        // clock_t start = clock();
        struct timespec time0 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time0);
#endif

        bool ret;

        static int speed_real = 0, mileage_real = 0; // 单位 mm/s mm

#ifdef COMM_ENABLED
        if (!replay_enabled) {
            // 串口通讯数据接收（从 TC264 端）
            // TODO 对串口数据接收过程计时，评估对性能的影响
            comm_type_t type;
            uint8_t payload[10]; // 数据

            // 尽量清空读缓冲区（同步两台设备的周期），并且返回最后一组数据
            bool last_ret = true; // 默认为没有数据（发生错误）
            ret = comm_recv_poll(&type, payload);
            while (!ret) {
                if (type == COMM_TYPE_HELLO_FROM_TC264) { // 上电通知 // 这个数据包不能被忽略
                    go_flag = 1;
                }
                last_ret = ret;
                ret = comm_recv_poll(&type, payload);
            }
            ret = last_ret;

            if (!ret) { // success
                if (type == COMM_TYPE_PING) { // 串口测试
                    // pong back
                    printf("pong %02x\n", payload[0]);
                    ret = comm_send_blocking(COMM_TYPE_PONG, payload);
                    assert(!ret);
                } else if (type == COMM_TYPE_UPDATE_FROM_TC264) { // 编码器数据更新
                    mileage_real = (payload[0]) + (payload[1] << 8) + (payload[2] << 16); // 小端序
                    speed_real = (double)payload[3] / 256 * 6000; // 解压缩
                }
            }
        }
#endif

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time2 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time2);
        int time2_diff = calc_time_diff(time2, time0);
#endif

        // 图像采集（新）
        static void *buf[60000];
        int len;
        cap_grab(buf, &len);
        assert(len < 60000);

        // 忽略第一帧
        static bool is_first_frame = true;
        if (!replay_enabled && is_first_frame) {
            is_first_frame = false;

            if (record_enabled && meta_enabled) {
                ret = cap_meta_record("", 0);
                assert(!ret);
            }

            continue;
        }

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time3 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time3);
        int time3_diff = calc_time_diff(time3, time2);
#endif

        // Ref: https://github.com/opencv/opencv/blob/4.x/modules/videoio/src/cap_v4l.cpp
        cv::Mat frame(cv::Size(CAP_WIDTH, CAP_HEIGHT), CV_8UC3);
        cv::imdecode(cv::Mat(1, len, CV_8U, buf), cv::IMREAD_COLOR, &frame);

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time4 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time4);
        int time4_diff = calc_time_diff(time4, time3);
#endif

        // 裁剪图像
        cv::resize(frame, frame, cv::Size(400, 300), 0, 0, cv::INTER_LINEAR);
        frame = frame(cv::Rect(0, 0, IMG_W, IMG_H));

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time5 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time5);
        int time5_diff = calc_time_diff(time5, time4);
#endif

        if (debug_enabled && replay_enabled) {
            show_pos = cap_replay_get_image_id();
            printf("#%d:\n", show_pos);
        }

#ifdef CONE_DEBUGING
        cv::Mat debug_image_perspective;
        if (debug_enabled) {
            // /*
            //     获取透视变换用矩阵（标定）
            //     车辆放在长直道，在画面内可见的赛道两侧边缘，远端与近端放置共计4个标志
            //     在图像中测量4个标志的像素点坐标，并且在现实中也测量4个标志的物理位置坐标（可以以mm为最小单位）
            //     将8个位置信息填入下表即可得出变换矩阵
            // */
            // cv::Point2f src_points[] = {
            //     cv::Point2f(17, 67), // （列，行）
            //     cv::Point2f(122, 67),
            //     cv::Point2f(79, 2),
            //     cv::Point2f(63, 1)
            // };
            // cv::Point2f dst_points[] = {
            //     cv::Point2f(100 - 22, 200),
            //     cv::Point2f(100 + 22, 200),
            //     cv::Point2f(100 + 22, -50),
            //     cv::Point2f(100 - 22, -50)
            // };
            // cv::Mat M = cv::getPerspectiveTransform(src_points, dst_points);
            // std::cout << M << std::endl;
            // std::cout << M.at<double>(0, 1) << std::endl; // 用于辅助识别输出的行列

            // memcpy(my_perspective_m, M.isContinuous() ? M.data : M.clone().data, sizeof my_perspective_m);

            cv::warpPerspective(frame, debug_image_perspective, cv::Mat(3, 3, CV_64F, my_perspective_m), cv::Size(200, 200), cv::INTER_LINEAR);
        }
#endif

        // 分离绿色通道作为灰度图像
        cv::Mat frame_splited[3];
        cv::split(frame, frame_splited);
        // cv::blur(frame_splited[1], frame_splited[1], cv::Size(3, 3)); // 减少噪点
        memcpy(track_img, frame_splited[1].isContinuous() ? frame_splited[1].data : frame_splited[1].clone().data, sizeof track_img); // Mat 转数组

        // 锥桶识别用图像
        for (int i = 0; i < frame.rows; ++i) {
            cv::Vec3b *pixel = frame.ptr<cv::Vec3b>(i); // point to first pixel in row
            for (int j = 0; j < frame.cols; ++j) {
                // red - green
                int diff = (int)pixel[j][2] - pixel[j][1];
                if (diff < 0) diff = 0;
                cone_img[i][j] = diff;
            }
        }

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time6 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time6);
        int time6_diff = calc_time_diff(time6, time5);
#endif

        bool do_timer = false;
        if (!replay_enabled) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timer_start).count() > 100) { // 每 0.1 s
                timer_start = std::chrono::steady_clock::now();
                do_timer = true;
            }
        }

        // 状态恢复及保存
        if (meta_enabled) {
            if (record_enabled) {
                // 保存状态到文件中
                static char buf[100];
                int len = sprintf(buf, "%d %d %d %d", (int)do_timer, (int)go_flag, mileage_real, speed_real);
                assert(len >= 0);
                assert(len <= 100);
                ret = cap_meta_record(buf, len);
                assert(!ret);
            } else if (replay_enabled) {
                // 从文件中恢复状态
                static char buf[100];
                int len;
                ret = cap_meta_replay(buf, &len);
                assert(!ret);
                buf[len] = 0; // scanf 依赖 \0 来识别字符串结束
                int tmp[10];
                ret = sscanf(buf, "%d%d%d%d", &tmp[0], &tmp[1], &mileage_real, &speed_real);
                assert(len >= 0);
                do_timer = tmp[0];
                go_flag = tmp[1];
            }
        }

        if (do_timer) {
            do_timer = false;

            timer();
        }

        if (replay_enabled && !meta_enabled) { // 向下兼容，赋一个默认参数
            gogo_flag = 100;
        }

        if (gogo_flag <= 40) {
            if (go_flag) {
                middle_line[control_line] = 0;
                gogo_flag++;
            }
        } else {
            track_main();
            cone_main();
            branch_main();

            if (cone_status != CONE_STATUS_NONE && cone_status != CONE_STATUS_IN_PRE) {
                for (int i = 0; i < IMG_H; ++i) {
                    middle_line[i] = cone_guide[i];
                }
            } else if (branch_status != BRANCH_STATUS_NONE) {
                for (int i = 0; i < IMG_H; ++i) {
                    middle_line[i] = branch_guide[i];
                }
            } else {
                // 填充传统算法
            }
        }

        element = 0;
        if (cone_status != CONE_STATUS_NONE && cone_status != CONE_STATUS_IN_PRE) {
            element = 3;
        } else if (branch_status == BRANCH_STATUS_IN || branch_status == BRANCH_STATUS_IN_STABLE || branch_status == BRANCH_STATUS_STAY) {
            element = 4;
        } else if (branch_status == BRANCH_STATUS_OUT || branch_status == BRANCH_STATUS_OUT_STABLE) {
            element = 5;
        }
        if (garage_stop_cnt > 3) {
            // element = 9;
            return 0;
        }
        
#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time7 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time7);
        int time7_diff = calc_time_diff(time7, time6);
#endif

#ifdef COMM_ENABLED
        if (!replay_enabled) {
            uint8_t payload[10]; // 数据
            // 串口通信数据发送
            // TODO 对串口数据发送过程计时，评估对性能的影响
            memset(payload, 0, sizeof payload);
            payload[0] = middle_line[control_line];
            payload[1] = element; // uint8_t 类型的类型数据 // TODO 去更改
            payload[2] = 0; // 传送其他信息啥的
            ret = comm_send_blocking(COMM_TYPE_UPDATE_TO_TC264, payload);
            assert(!ret);
        }
#endif

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time8 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time8);
        int time8_diff = calc_time_diff(time8, time7);
#endif

#ifdef VOFA_ENABLED
        if (debug_enabled) {
            // vofa 测试
            static uint8_t vofa_test_cnt = 0;
            ++vofa_test_cnt;

            char vofa_buf[100];
            int vofa_buf_len = sprintf(vofa_buf, "test: %d, %d\n", vofa_test_cnt, -vofa_test_cnt);
            ret = vofa_send(vofa_buf, vofa_buf_len);
            assert(!ret);
        }
#endif

#ifdef MAIN_TIMING
        // 时间测量
        struct timespec time9 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time9);
        int time9_diff = calc_time_diff(time9, time8);
#endif

#ifdef CONE_DEBUGING
        if (debug_enabled) {
            if (cone_status == CONE_STATUS_NONE) {
                printf(ANSI_COLOR_RED "CONE_STATUS_NONE" ANSI_COLOR_RESET "\n");
            } else if (cone_status == CONE_STATUS_IN_PRE) {
                printf(ANSI_COLOR_RED "CONE_STATUS_IN_PRE" ANSI_COLOR_RESET "\n");
            } else if (cone_status == CONE_STATUS_IN) {
                printf(ANSI_COLOR_RED "CONE_STATUS_IN" ANSI_COLOR_RESET "\n");
            } else if (cone_status == CONE_STATUS_IN_AFTER) {
                printf(ANSI_COLOR_RED "CONE_STATUS_IN_AFTER" ANSI_COLOR_RESET "\n");
            } else if (cone_status == CONE_STATUS_STAY) {
                printf(ANSI_COLOR_RED "CONE_STATUS_STAY" ANSI_COLOR_RESET "\n");
            } else if (cone_status == CONE_STATUS_OUT) {
                printf(ANSI_COLOR_RED "CONE_STATUS_OUT" ANSI_COLOR_RESET "\n");
            } else if (cone_status == CONE_STATUS_OUT_AFTER) {
                printf(ANSI_COLOR_RED "CONE_STATUS_OUT_AFTER" ANSI_COLOR_RESET "\n");
            }

            cv::Mat debug_image_track_connected = cv::Mat::zeros(cv::Size(IMG_W, IMG_H), CV_8UC3);
            // output
            cv::RNG track_rng(10086);
            std::vector<cv::Vec3b> track_colors(track_connected_cnt + 1);
            for (int i = 1; i <= track_connected_cnt; ++i) {
                if (!track_connected_stat_sorted[i]->invalid_flag) {
                    // 生成随机颜色
                    track_colors[track_connected_stat_sorted[i]->id] = cv::Vec3b(track_rng.uniform(0, 255), track_rng.uniform(0, 255), track_rng.uniform(0, 255));

                    // std::cout << cv::format("%d\t%d\t%d\t%d\t%d\t",
                    //     track_connected_stat_sorted[i]->id,
                    //     track_connected_stat_sorted[i]->size,
                    //     track_connected_stat_sorted[i]->color,
                    //     track_connected_stat_sorted[i]->center.a,
                    //     track_connected_stat_sorted[i]->center.b
                    // );

                    // if (track_connected_stat_sorted[i]->track_flag) {
                    //     std::cout << "white\t";
                    // }

                    // std::cout << std::endl;
                }
            }
            // std::cout << "----------------------------------------" << std::endl;

            // 以不同颜色标记出不同的连通域
            for (int i = 0; i < IMG_H; ++i) {
                for (int j = 0; j < IMG_W; ++j) {
                    int id = track_connected_img[i][j];
                    if (id != 0 && !track_connected_stat[id].invalid_flag) { // 不改变背景的黑色
                        if (track_connected_stat[id].track_flag) {
                            debug_image_track_connected.at<cv::Vec3b>(i, j) = cv::Vec3b(0xff, 0xff, 0xff);
                        } else {
                            debug_image_track_connected.at<cv::Vec3b>(i, j) = track_colors[id];
                        }
                    }
                }
            }

            cv::Mat debug_image_cone_connected = cv::Mat::zeros(cv::Size(IMG_W, IMG_H), CV_8UC3);
            // output
            cv::RNG cone_rng(10086);
            std::vector<cv::Vec3b> cone_colors(cone_connected_cnt + 1);
            for (int i = 1; i <= cone_connected_cnt; ++i) {
                if (!cone_connected_stat[i].invalid_flag) {
                    // 生成随机颜色
                    cone_colors[cone_connected_stat[i].id] = cv::Vec3b(cone_rng.uniform(0, 255), cone_rng.uniform(0, 255), cone_rng.uniform(0, 255));

                    std::cout << cv::format("%d\t%d\t%d\t%d\t%d\t",
                        cone_connected_stat[i].id,
                        cone_connected_stat[i].size,
                        cone_connected_stat[i].color,
                        cone_connected_stat[i].bottom.a,
                        cone_connected_stat[i].bottom.b
                    );

                    if (cone_connected_stat[i].cone_flag) {
                        std::cout << "cone\t";
                    }

                    std::cout << std::endl;
                }
            }
            std::cout << "----------------------------------------" << std::endl;

            // 以不同颜色标记出不同的连通域
            for (int i = 0; i < IMG_H; ++i) {
                for (int j = 0; j < IMG_W; ++j) {
                    int id = cone_connected_img[i][j];
                    if (id != 0 && !cone_connected_stat[id].invalid_flag) { // 不改变背景的黑色
                        if (cone_connected_stat[id].cone_flag) {
                            debug_image_cone_connected.at<cv::Vec3b>(i, j) = cv::Vec3b(0xff, 0xff, 0xff);
                        } else {
                            debug_image_cone_connected.at<cv::Vec3b>(i, j) = cone_colors[id];
                        }
                    }
                }
            }

            for (int i = 1; i <= cone_connected_cnt; ++i) {
                if (cone_connected_stat[i].cone_flag) { // 不改变背景的黑色
                    pair_t p = cone_connected_stat[i].bottom;

                    // 中心位置绘制
                    cv::circle(debug_image_track_connected, cv::Point(
                            p.b,
                            p.a),
                        1, cv::Scalar(127, 127, 127), 2, 8, 0);

                    // // 序号绘制
                    // cv::putText(debug_image_track_connected, cv::format("%d", i), cv::Point(p.b, p.a), cv::FONT_HERSHEY_SIMPLEX, 0.25, cv::Scalar(255, 255, 0));

                    // // 矩形边框
                    // int x = stats.at<int>(i, cv::CC_STAT_LEFT);
                    // int y = stats.at<int>(i, cv::CC_STAT_TOP);
                    // int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
                    // int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
                    // int area = stats.at<int>(i, cv::CC_STAT_AREA);

                    // // 外接矩形
                    // cv::Rect rect(x, y, w, h);
                    // cv::rectangle(debug_image_cone_connected, rect, cone_colors[i], 1, 8, 0);
                    // cv::putText(debug_image_cone_connected, cv::format("%d", i), cv::Point(center_x, center_y),
                    //     cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 1);
                    // std::cout << "i: " << i << ", area: " << area << std::endl;
                }
            }

            // 绘制关键点
            if (cone_status == CONE_STATUS_IN) {
                cv::circle(debug_image_track_connected, cv::Point(
                        cone_entry2.b,
                        cone_entry2.a),
                    1, cv::Scalar(127, 0, 0), 2, 8, 0);
            }

            // 绘制关键点
            if (cone_status == CONE_STATUS_OUT) {
                cv::circle(debug_image_track_connected, cv::Point(
                        cone_exit2.b,
                        cone_exit2.a),
                    1, cv::Scalar(127, 0, 0), 2, 8, 0);
            }

            // 绘制排序结果
            if (cone_status != CONE_STATUS_NONE) {
                for (int i = 1; i <= cone_connected_cnt; ++i) {
                    int id = cone_connected_stat_sorted[i]->id;
                    if (cone_connected_stat[id].cone_flag) {
                        pair_t p = cone_connected_stat[id].bottom;
                        cv::putText(debug_image_track_connected, cv::format("%d", i), cv::Point(p.b, p.a), cv::FONT_HERSHEY_SIMPLEX, 0.25, cv::Scalar(0, 255, 255));
                    }
                }
            }

            if (cone_status != CONE_STATUS_NONE) {
                for (int i = 0; i < IMG_H; ++i) {
                    if (cone_border_right[i] != -1) {
                        cv::circle(debug_image_track_connected, cv::Point(cone_border_right[i], i),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                        pair_t pers_p = my_perspective_transform({ i, cone_border_right[i] });
                        cv::circle(debug_image_perspective, cv::Point(pers_p.b, pers_p.a),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                    }

                    if (cone_border_left[i] != -1) {
                        cv::circle(debug_image_track_connected, cv::Point(cone_border_left[i], i),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                        pair_t pers_p = my_perspective_transform({ i, cone_border_left[i] });
                        cv::circle(debug_image_perspective, cv::Point(pers_p.b, pers_p.a),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                    }

                    if (cone_guide[i] != -1) {
                        cv::circle(debug_image_track_connected, cv::Point(cone_guide[i], i),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                        pair_t pers_p = my_perspective_transform({ i, cone_guide[i] });
                        cv::circle(debug_image_perspective, cv::Point(pers_p.b, pers_p.a),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                    }
                }
            }

            if (cone_status == CONE_STATUS_NONE) {
                for (int i = 0; i < IMG_H; ++i) {
                    if (track_guide[i] != -1) {
                        cv::circle(debug_image_track_connected, cv::Point(track_guide[i], i),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                        pair_t pers_p = my_perspective_transform({ i, track_guide[i] });
                        cv::circle(debug_image_perspective, cv::Point(pers_p.b, pers_p.a),
                            0, cv::Scalar(127, 127, 127), -1, 8, 0);
                    }
                }
            }

            // show
            my_image_show("orig", frame);
            my_image_show("perspective", debug_image_perspective);
            // my_image_show("channel_b", image_rgbchannel[0]);
            // my_image_show("channel_g", image_rgbchannel[1]);
            // my_image_show("channel_r", image_rgbchannel[2]);
            my_img_show("track", track_img);
            // my_img_show("track_border", track_not_border_img);
            my_image_show("track_connected", debug_image_track_connected);
            my_img_show("cone", cone_img);
            // my_img_show("cone_border", cone_thresholded_img);
            my_image_show("cone_connected", debug_image_cone_connected);

            // 设置回调函数
            my_mouse_data.rng = cv::RNG(12345); // 固定随机数种子使得每次颜色差不多
            my_mouse_data.name = "track_connected";
            my_mouse_data.image = debug_image_track_connected; // 潜拷贝 .clone() 为深拷贝
            my_mouse_data.pers_name = "perspective";
            my_mouse_data.pers_image = debug_image_perspective; // 潜拷贝 .clone() 为深拷贝
            cv::setMouseCallback(my_mouse_data.name, on_mouse, &my_mouse_data); // 通过 userdata 传递画布
        }
#endif

#ifdef BRANCH_DEBUGING
        if (debug_enabled) {
            // // 转换灰度化的图片为画布
            // cv::Mat debug_image_branch(IMG_H, IMG_W, CV_8UC1, img);
            // cv::cvtColor(debug_image_branch, debug_image_branch, cv::COLOR_GRAY2BGR);

            // for (int i = 0; i < IMG_H; ++i) {
            //     if (left_black[i] >= 0 && left_black[i] < IMG_W)
            //         cv::circle(debug_image_branch, cv::Point(left_black[i], i), 0, cv::Scalar(0, 127, 0), -1, 8, 0);
            // }

            // for (int i = 0; i < IMG_H; ++i) {
            //     if (left_black[i] >= 0 && left_black[i] < IMG_W)
            //         cv::circle(debug_image_branch, cv::Point(right_black[i], i), 0, cv::Scalar(0, 255, 0), -1, 8, 0);
            // }

            // if (left_turning_point >= 0 && left_turning_point < IMG_W) {
            //     cv::circle(debug_image_branch, cv::Point(left_black[left_turning_point], left_turning_point), 2, cv::Scalar(0, 127, 127), 2, 8, 0);
            // } else {
            //     printf("left_turning_point not found\n");
            // }

            // if (right_turning_point >= 0 && right_turning_point < IMG_W) {
            //     cv::circle(debug_image_branch, cv::Point(right_black[right_turning_point], right_turning_point), 2, cv::Scalar(0, 255, 255), 2, 8, 0);
            // } else {
            //     printf("right_turning_point not found\n");
            // }

            // my_image_show("branch", debug_image_branch);

            if (branch_status == BRANCH_STATUS_NONE) {
                printf(ANSI_COLOR_GREEN "BRANCH_STATUS_NONE" ANSI_COLOR_RESET "\n");
            } else if (branch_status == BRANCH_STATUS_IN) {
                printf(ANSI_COLOR_GREEN "BRANCH_STATUS_IN" ANSI_COLOR_RESET "\n");
            } else if (branch_status == BRANCH_STATUS_IN_STABLE) {
                printf(ANSI_COLOR_GREEN "BRANCH_STATUS_IN_STABLE" ANSI_COLOR_RESET "\n");
            } else if (branch_status == BRANCH_STATUS_STAY) {
                printf(ANSI_COLOR_GREEN "BRANCH_STATUS_STAY" ANSI_COLOR_RESET "\n");
            } else if (branch_status == BRANCH_STATUS_OUT) {
                printf(ANSI_COLOR_GREEN "BRANCH_STATUS_OUT" ANSI_COLOR_RESET "\n");
            } else if (branch_status == BRANCH_STATUS_OUT_STABLE) {
                printf(ANSI_COLOR_GREEN "BRANCH_STATUS_OUT_STABLE" ANSI_COLOR_RESET "\n");
            }

            cv::Mat debug_image_branch(IMG_H, IMG_W, CV_8UC1, track_img);
            cv::cvtColor(debug_image_branch, debug_image_branch, cv::COLOR_GRAY2BGR);
            // for (int i = 0; i < IMG_H; ++i) {
            //     if (track_guide[i] != -1) {
            //         cv::circle(debug_image_branch, cv::Point(track_guide[i], i), 0, cv::Scalar(127, 127, 127), -1, 8, 0);
            //     }
            //     if (track_border_left[i] != -1) {
            //         cv::circle(debug_image_branch, cv::Point(track_border_left[i], i), 0, cv::Scalar(0, 127, 0), -1, 8, 0);
            //     }
            //     if (track_border_right[i] != -1) {
            //         cv::circle(debug_image_branch, cv::Point(track_border_right[i], i), 0, cv::Scalar(0, 255, 0), -1, 8, 0);
            //     }
            // }
            // for (int i = 0; i < IMG_H; ++i) {
            //     if (branch_border_on_diamond_left[i] != -1) {
            //         cv::circle(debug_image_branch, cv::Point(branch_border_on_diamond_left[i], i), 0, cv::Scalar(0, 127, 0), -1, 8, 0);
            //     }
            //     if (branch_border_on_diamond_right[i] != -1) {
            //         cv::circle(debug_image_branch, cv::Point(branch_border_on_diamond_right[i], i), 0, cv::Scalar(0, 255, 0), -1, 8, 0);
            //     }
            // }

            if (branch_status == BRANCH_STATUS_IN || branch_status == BRANCH_STATUS_IN_STABLE) {
                if (branch_corner_bottom_right.a != -1) {
                    cv::circle(debug_image_branch, cv::Point(branch_corner_bottom_right.b, branch_corner_bottom_right.a), 2, cv::Scalar(0, 255, 255), 2, 8, 0);
                }

                if (branch_corner_in.a != -1) {
                    cv::circle(debug_image_branch, cv::Point(branch_corner_in.b, branch_corner_in.a), 2, cv::Scalar(255, 0, 0), 2, 8, 0);
                }
            }

            if (branch_status == BRANCH_STATUS_STAY || branch_status == BRANCH_STATUS_OUT || branch_status == BRANCH_STATUS_OUT_STABLE) {
                if (branch_corner_top_right.a != -1) {
                    cv::circle(debug_image_branch, cv::Point(branch_corner_top_right.b, branch_corner_top_right.a), 2, cv::Scalar(0, 255, 255), 2, 8, 0);
                }
            }

            // 绘制边界线
            for (int i = 0; i < IMG_H; ++i) {
                if (branch_border_right[i] != -1) {
                    cv::circle(debug_image_branch, cv::Point(branch_border_right[i], i), 0, cv::Scalar(0, 255, 0), -1, 8, 0);
                }
                if (branch_guide[i] != -1) {
                    cv::circle(debug_image_branch, cv::Point(branch_guide[i], i), 0, cv::Scalar(127, 127, 127), -1, 8, 0);
                }
            }

            my_image_show("branch", debug_image_branch);
        }
#endif

#ifdef IMG_DEBUGING
        if (debug_enabled) {
            // 转换灰度化的图片为画布
            cv::Mat frame_show(IMG_H, IMG_W, CV_8UC1, img);
            cv::cvtColor(frame_show, frame_show, cv::COLOR_GRAY2BGR);

            draw_line(frame_show, 0, control_line, 139, control_line, CV_COLOR_RED);

            static int speed_read_filtered = 0;
            // speed_read_filtered = speed_read_filtered * (1 - 0.2) + speed_real * 0.2; // 滤波
            speed_read_filtered = speed_real; // 滤波
            printf("%01.2fm/s %05dmm\n", (double)speed_read_filtered / 1000, mileage_real);
            cv::putText(frame_show, cv::format("%01.2fm/s %05dmm", (double)speed_read_filtered / 1000, mileage_real), cv::Point(2, 8), cv::FONT_HERSHEY_SIMPLEX, 0.25, cv::Scalar(127, 127, 127));

            my_image_show("img", frame_show);

            cv::waitKey(1);
        }
#endif

#ifdef IMG_DEBUGING_LIVE_WATCH
        if (debug_enabled) {
            live_watch("garage", garage_stop_cnt);

            // printf("\n");
            printf("\033[1A"); // 先回到上一行
            printf("\033[K");
        }
#endif

        if (debug_enabled) {
            // 播放控制
            static bool play = false;
            int key = cv::waitKey(play ? 1 : 0);
            if (key == 'q') {
                break;
            } else if (key == 's') {
                play = true;
            } else if (replay_enabled) {
                if (key == 'a') {
                    play = false;
                    cap_replay_go(-1);
                } else if (key == 'z') {
                    play = false;
                    cap_replay_go(-10);
                } else if (key == 'c') {
                    play = false;
                    cap_replay_go(10);
                } else {
                    cap_replay_go(1);
                }
            }
        } else if (replay_enabled) {
            cap_replay_go(1);
        }

        // 不输出，快速计算到指定帧
        if (replay_fast_forward != -1 && cap_replay_get_image_id() >= replay_fast_forward - 1) {
            debug_enabled = true;
            replay_fast_forward = -1;
        }

#ifdef MAIN_TIMING
        // 时间测量
        // int time_all = (int)((clock() - start) * 1000 / CLOCKS_PER_SEC);
        // if (time_all >= 8)
        //     printf("%dms\n", time_all);
        struct timespec time1 = {0, 0};
        clock_gettime(CLOCK_MONOTONIC, &time1);
        int time_all = calc_time_diff(time1, time0);
        if (time_all >= 9000)
            printf(ANSI_COLOR_YELLOW "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t(us)\n" ANSI_COLOR_RESET,
                time_all, time2_diff, time3_diff, time4_diff, time5_diff, time6_diff, time7_diff, time8_diff, time9_diff
            );
#endif
    }

    cap_deinit();

    return 0;
}
