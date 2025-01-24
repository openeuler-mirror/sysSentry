/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: ebpf collector program
 * Author: Zhang Nan
 * Create: 2024-09-27
 */
#ifndef __EBPFCOLLECTOR_H
#define __EBPFCOLLECTOR_H

typedef long long unsigned int u64;
typedef unsigned int u32;

#define MAX_IO_TIME 130
#define IO_DUMP_THRESHOLD 120
#define THRESHOLD 1000000000
#define DURATION_THRESHOLD 500000000

#define RWBS_LEN    8

#define REQ_OP_BITS 8
#define REQ_OP_MASK ((1 << REQ_OP_BITS) - 1)
#define REQ_FUA         (1ULL << __REQ_FUA)
#define REQ_RAHEAD      (1ULL << __REQ_RAHEAD)
#define REQ_SYNC        (1ULL << __REQ_SYNC)
#define REQ_META        (1ULL << __REQ_META)
#define REQ_PREFLUSH    (1ULL << __REQ_PREFLUSH)
#define REQ_OP_READ 0
#define REQ_OP_WRITE 1
#define REQ_OP_FLUSH 2
#define REQ_OP_DISCARD 3
#define REQ_OP_SECURE_ERASE 5
#define REQ_OP_WRITE_SAME 7
#define MAP_SIZE   15

#define RWBS_LEN    8
#define MINORBITS    20
#define MINORMASK    ((1U << MINORBITS) - 1)

#define MAJOR(dev)    ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)    ((unsigned int) ((dev) & MINORMASK))

#ifndef NULL
#define NULL (void *)0
#endif

// 阶段
#define STAGE_RQ_DRIVER 1
#define STAGE_BIO       2
#define STAGE_WBT       3
#define STAGE_GET_TAG   4

// 时期
#define PERIOD_START    1
#define PERIOD_END      2

// 错误码
#define ERROR_MAJOR_ZERO 1
#define ERROR_KEY_OVERFLOW 2
#define ERROR_KEY_EXIST 3
#define ERROR_UPDATE_FAIL 4
#define ERROR_KEY_NOEXIST 5

enum stage_type {
    BIO=0,
    WBT,
    GET_TAG,
    DEADLINE,
    BFQ,
    KYBER,
    RQ_DRIVER,
    MAX_STAGE_TYPE,
};

struct stage_data {
    u64 start_count;		
    u64 finish_count;		
    u64 finish_over_time;	
    u64 duration;			
    int major;
    int first_minor;
    char io_type[RWBS_LEN];
};

struct io_counter { 
    u64 duration; 
    u64 start_time; 
    u32 isend; 
    int major;
    int first_minor; 
};

struct update_params {
    int major;
    int first_minor;
    unsigned int cmd_flags;
    u64 curr_start_range;
};

struct time_range_io_count
{
    u32 count[MAP_SIZE];
};

struct event {
    u32 stage;
    u64 period;
    u32 err; 
};

#endif /* __EBPFCOLLECTOR_H */
