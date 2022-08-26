#ifndef _COMM_H_
#define _COMM_H_

#include <stdbool.h>
#include <stdint.h>

#define COMM_PAYLOAD_SIZE_MAX 10

typedef enum {
    COMM_TYPE_PING = 0,
    COMM_TYPE_PONG,
    COMM_TYPE_UPDATE_TO_TC264,
    COMM_TYPE_UPDATE_FROM_TC264,
    COMM_TYPE_HELLO_FROM_TC264,
} comm_type_t;

extern int comm_payload_size[];

bool comm_send_blocking(comm_type_t type, const uint8_t payload[]);

bool comm_recv_poll(comm_type_t *type, uint8_t payload[]);

bool comm_init(void);

#endif