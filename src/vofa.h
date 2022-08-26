#ifndef _VOFA_H_
#define _VOFA_H_

#include <stdbool.h>
#include <stdint.h>

bool vofa_init(void);
bool vofa_send(char *buf, int buf_len);

#endif