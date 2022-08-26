#ifndef _CAP_H_
#define _CAP_H_

#include <stdbool.h>
#include <stdint.h>

#define CAP_WIDTH 1280
#define CAP_HEIGHT 720
#define CAP_FPS 60

bool cap_init(bool is_replay_, bool is_record_, bool have_meta_);
bool cap_deinit(void);
bool cap_grab(void *raw_image, int *raw_image_size);

int cap_replay_get_image_id(void);
bool cap_replay_go(int step);

bool cap_meta_record(const char *buf, int len);
bool cap_meta_replay(char *buf, int *len);

void convert_record_from_splitted_file(void);

#endif