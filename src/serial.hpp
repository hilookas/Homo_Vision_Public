#ifndef _SERIAL_HPP_
#define _SERIAL_HPP_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

bool serial_init(void);

bool serial_send_blocking(uint8_t c);

bool serial_recv_poll(uint8_t *c);

#ifdef __cplusplus
}
#endif

#endif