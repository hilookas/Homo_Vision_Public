#include "comm.h"
#include "serial.hpp"
#include <string.h>
#include <assert.h>

// 不同类型的数据包大小
int comm_payload_size[] = {
    1, // COMM_TYPE_PING
    1, // COMM_TYPE_PONG
    3, // COMM_TYPE_UPDATE_TO_TC264
    4, // COMM_TYPE_UPDATE_FROM_TC264
    0, // COMM_TYPE_HELLO_FROM_TC264
};

// 堵塞发送一个数据包
// type 传递数据类型，payload 传递实际数据
// 返回是否发生错误
bool comm_send_blocking(comm_type_t type, const uint8_t payload[]) {
    bool ret;
    ret = serial_send_blocking(0x5A); // 0b01011010 as header
    if (ret) return true;
    ret = serial_send_blocking((uint8_t)type); // 0b01011010 as header
    if (ret) return true;
    for (int i = 0; i < comm_payload_size[type]; ++i) {
        ret = serial_send_blocking(payload[i]);
        if (ret) return true;
    }
    return false;
}

static uint8_t recv_buf[2 + COMM_PAYLOAD_SIZE_MAX];
static int recv_buf_p;

// 尝试接收一个数据包
// 如果没有发生错误，则 *type 内为接收到的数据包类型，payload 为接收到的实际数据
// 返回是否发生错误
// 这个函数发生错误十分正常，在没有接收到数据的时候，就会返回错误（不阻塞）
bool comm_recv_poll(comm_type_t *type, uint8_t payload[]) {
    bool ret;
    while (true) {
        uint8_t buf;
        ret = serial_recv_poll(&buf);
        if (ret) return true; // 没有新数据
        if (recv_buf_p == 0 && buf != 0x5A) continue; // 无效数据
        recv_buf[recv_buf_p++] = buf;
        assert(recv_buf_p <= sizeof recv_buf); // 缓冲区太小存不下完整数据包
        assert(recv_buf_p != 2 || recv_buf[1] < (sizeof comm_payload_size) / (sizeof comm_payload_size[0])); // 收到未知类型的数据包
        if (recv_buf_p >= 2 && recv_buf_p == 2 + comm_payload_size[recv_buf[1]]) { // 数据包到结尾了
            // 复制数据输出
            *type = recv_buf[1];
            memcpy(payload, recv_buf + 2, sizeof comm_payload_size[recv_buf[1]]);
            // 清空缓冲区
            recv_buf_p = 0;
            break;
        }
        // TODO 增加超时机制
    }
    return false;
}

// 返回是否发生错误
bool comm_init(void) {
    bool ret = serial_init();
    if (ret) return true; // 没有新数据
    recv_buf_p = 0;
    return false;
}