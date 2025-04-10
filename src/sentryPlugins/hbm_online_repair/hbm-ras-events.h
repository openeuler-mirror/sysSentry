/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2025. All rights reserved.
 * Description: hbm non standard event decoding and processing
 * Author: luckky
 * Create: 2024-10-30
 */

#ifndef __HBM_RAS_EVENTS_H
#define __HBM_RAS_EVENTS_H

#include <stdint.h>
#include <time.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define ENDIAN KBUFFER_ENDIAN_LITTLE
#else
    #define ENDIAN KBUFFER_ENDIAN_BIG
#endif

#define MAX_PATH 1024
#define DEFAULT_PAGE_SIZE  4096

struct ras_events {
    char              tracing[MAX_PATH + 1];
    struct tep_handle *pevent;
    int               page_size;
};

int toggle_ras_event(char *trace_dir, char *group, char *event, int enable);
int handle_ras_events(struct ras_events *ras);
struct ras_events *init_trace_instance(void);

#endif
