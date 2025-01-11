/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: ebpf collector program
 * Author: Zhang Nan
 * Create: 2024-09-27
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "ebpf_collector.h"

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10000);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct io_counter));
} blk_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 128);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct stage_data));
} blk_res SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10000);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct io_counter));
} bio_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 128);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct stage_data));
} bio_res SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10000);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct io_counter));
} wbt_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 128);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct stage_data));
} wbt_res SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1000);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u64));
} wbt_args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10000);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct io_counter));
} tag_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 128);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct stage_data));
} tag_res SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1000);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(u64));
} tag_args SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_IO_TIME);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct time_range_io_count));
} blk_res_2 SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_IO_TIME);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct time_range_io_count));
} bio_res_2 SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_IO_TIME);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct time_range_io_count));
} wbt_res_2 SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, MAX_IO_TIME);
    __uint(key_size, sizeof(u32));
    __uint(value_size, sizeof(struct time_range_io_count));
} tag_res_2 SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} ringbuf SEC(".maps");


static void log_event(u32 stage, u32 period, u32 err) {
    struct event *e;
    void *data = bpf_ringbuf_reserve(&ringbuf, sizeof(struct event), 0);
    if (!data)
        return;

    e = (struct event *)data;
    e->stage = stage;
    e->period = period;
    e->err = err;

    bpf_ringbuf_submit(e, 0);
}

static __always_inline void blk_fill_rwbs(char *rwbs, unsigned int op)
{
    switch (op & REQ_OP_MASK) {
    case REQ_OP_WRITE:
    case REQ_OP_WRITE_SAME:
        rwbs[0] = 'W';
        break;
    case REQ_OP_DISCARD:
        rwbs[0] = 'D';
        break;
    case REQ_OP_SECURE_ERASE:
        rwbs[0] = 'E';
        break;
    case REQ_OP_FLUSH:
        rwbs[0] = 'F';
        break;
    case REQ_OP_READ:
        rwbs[0] = 'R';
        break;
    default:
        rwbs[0] = 'N';
    }

    if (op & REQ_FUA) {
        rwbs[1] = 'F';
    } else {
        rwbs[1] = '#';
    }
    if (op & REQ_RAHEAD) {
        rwbs[2] = 'A';
    } else {
        rwbs[2] = '#';
    }
    if (op & REQ_SYNC) {
        rwbs[3] = 'S';
    } else {
        rwbs[3] = '#';
    }
    if (op & REQ_META) {
        rwbs[4] = 'M';
    } else {
        rwbs[4] = '#';
    }
}

static void update_curr_data_in_start(struct stage_data *curr_data, struct update_params *params) {
    if (curr_data && params) {
        curr_data->start_count += 1;
        curr_data->major = params->major;
        curr_data->first_minor = params->first_minor;
        blk_fill_rwbs(curr_data->io_type, params->cmd_flags);
    }
}

static void update_curr_data_in_finish(struct stage_data *curr_data, struct update_params *params, u64 duration) {
    if (curr_data && params) {
        curr_data->finish_count += 1;
        curr_data->major = params->major;
        curr_data->first_minor = params->first_minor;
        blk_fill_rwbs(curr_data->io_type, params->cmd_flags);
        if (duration > DURATION_THRESHOLD) { 
            curr_data->finish_over_time += 1;
        }
    }
}

static void init_io_counter(struct io_counter *counterp, int major, int first_minor) {
    if (counterp) {
        counterp->start_time = bpf_ktime_get_ns();
        counterp->major = major;
        counterp->first_minor = first_minor;
    }
}

int find_matching_key_rq_driver(int major, int first_minor) {
    int key = 0;
    for (size_t i = 0; i < MAP_SIZE; i++) {
        struct stage_data *curr_data = bpf_map_lookup_elem(&blk_res, &key);
        struct stage_data tmp_data;
        bpf_core_read(&tmp_data, sizeof(tmp_data), curr_data);
        if (tmp_data.major == major && tmp_data.first_minor == first_minor) {
            return key;
        }
        key++;
    }
    return key;
}

int find_matching_key_bio(int major, int first_minor) {
    int key = 0;
    for (size_t i = 0; i < MAP_SIZE; i++) {
        struct stage_data *curr_data = bpf_map_lookup_elem(&bio_res, &key);
        struct stage_data tmp_data;
        bpf_core_read(&tmp_data, sizeof(tmp_data), curr_data);
        if (tmp_data.major == major && tmp_data.first_minor == first_minor) {
            return key;
        }
        key++;
    }
    return key; 
}

int find_matching_key_wbt(int major, int first_minor) {
    int key = 0;
    for (size_t i = 0; i < MAP_SIZE; i++) {
        struct stage_data *curr_data = bpf_map_lookup_elem(&wbt_res, &key);
        struct stage_data tmp_data;
        bpf_core_read(&tmp_data, sizeof(tmp_data), curr_data);
        if (tmp_data.major == major && tmp_data.first_minor == first_minor) {
            return key;
        }
        key++;
    }
    return key; 
}

int find_matching_key_get_tag(int major, int first_minor) {
    int key = 0;
    for (size_t i = 0; i < MAP_SIZE; i++) {
        struct stage_data *curr_data = bpf_map_lookup_elem(&tag_res, &key);
        struct stage_data tmp_data;
        bpf_core_read(&tmp_data, sizeof(tmp_data), curr_data);
        if (tmp_data.major == major && tmp_data.first_minor == first_minor) {
            return key;
        }
        key++;
    }
    return key; 
}

// start rq_driver
SEC("kprobe/blk_mq_start_request")
int kprobe_blk_mq_start_request(struct pt_regs *regs) {
    struct request *rq;
    struct request_queue *q;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags;

    rq = (struct request *)PT_REGS_PARM1(regs);
    bpf_core_read(&q, sizeof(q), &rq->q);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &q->disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);
    bpf_core_read(&cmd_flags, sizeof(cmd_flags), &rq->cmd_flags);

    if (major == 0) {
        log_event(STAGE_RQ_DRIVER, PERIOD_START, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_rq_driver(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }
    
    struct io_counter *counterp, zero = {};
    init_io_counter(&zero, major, first_minor);
    counterp = bpf_map_lookup_elem(&blk_map, &rq); 
    if (counterp) {
        return 0;
    }

    long err = bpf_map_update_elem(&blk_map, &rq, &zero, BPF_NOEXIST);
    if (err) {
        log_event(STAGE_RQ_DRIVER, PERIOD_START, ERROR_UPDATE_FAIL);
        return 0;
    }

    u64 curr_start_range = zero.start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    };

    struct stage_data *curr_data;
    curr_data = bpf_map_lookup_elem(&blk_res, &key);
    if (!curr_data) {
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 0,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&blk_res, &key, &new_data, 0);
    } else {
        update_curr_data_in_start(curr_data, &params);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&blk_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) {
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&blk_res_2, &curr_start_range, &new_data, 0); 
    } else {
        if (key < MAP_SIZE && key >= 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }
    return 0;
}

// finish rq_driver
SEC("kprobe/blk_mq_free_request")
int kprobe_blk_mq_free_request(struct pt_regs *regs)
{
    struct request *rq;
    struct request_queue *q;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags;
    struct io_counter *counterp;

    rq = (struct request *)PT_REGS_PARM1(regs);
    bpf_core_read(&q, sizeof(q), &rq->q);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &q->disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);
    bpf_core_read(&cmd_flags, sizeof(cmd_flags), &rq->cmd_flags);

    if (major == 0) {
        log_event(STAGE_RQ_DRIVER, PERIOD_END, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_rq_driver(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    counterp = bpf_map_lookup_elem(&blk_map, &rq);
    if (!counterp) {
        return 0;
    }

    u64 duration = bpf_ktime_get_ns() - counterp->start_time;
    u64 curr_start_range = counterp->start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    };

    struct stage_data *curr_data;
    curr_data = bpf_map_lookup_elem(&blk_res, &key);
    if (curr_data == NULL && duration > DURATION_THRESHOLD) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 1,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&blk_res, &key, &new_data, 0);
    } else if (curr_data == NULL) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&blk_res, &key, &new_data, 0); 
    } else {
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, duration);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&blk_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) { 
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&blk_res_2, &curr_start_range, &new_data, 0);
    } else {
        if (key < MAP_SIZE && curr_data_time_range->count[key] > 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], -1);
        }
    }
    bpf_map_delete_elem(&blk_map, &rq); 
    return 0;
}

// start bio
SEC("kprobe/blk_mq_submit_bio") 
int kprobe_blk_mq_submit_bio(struct pt_regs *regs)
{
    struct bio *bio;
    struct block_device *bd;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags;

    bio = (struct bio *)PT_REGS_PARM1(regs);
    bpf_core_read(&bd, sizeof(bd), &bio->bi_bdev);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &bd->bd_disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);
    bpf_core_read(&cmd_flags, sizeof(cmd_flags), &bio->bi_opf);

    if (major == 0) {
        log_event(STAGE_BIO, PERIOD_START, ERROR_MAJOR_ZERO);
        return 0;
    }

    int key = find_matching_key_bio(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    struct io_counter *counterp, zero = {};
    init_io_counter(&zero, major, first_minor);

    counterp = bpf_map_lookup_elem(&bio_map, &bio); 
    if (counterp) {
        return 0;
    }

    long err = bpf_map_update_elem(&bio_map, &bio, &zero, BPF_NOEXIST); 
    if (err) {
        log_event(STAGE_BIO, PERIOD_START, ERROR_UPDATE_FAIL);
        return 0;
    }

    u64 curr_start_range = zero.start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    }; 

    struct stage_data *curr_data;
    curr_data = bpf_map_lookup_elem(&bio_res, &key);
    if (curr_data == NULL) {
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 0,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&bio_res, &key, &new_data, 0);
    } else {
        update_curr_data_in_start(curr_data, &params);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&bio_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) {
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&bio_res_2, &curr_start_range, &new_data, 0); 
    } else {
        if (key < MAP_SIZE && key >= 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }
    return 0; 
}

// finish bio
SEC("kprobe/bio_endio") 
int kprobe_bio_endio(struct pt_regs *regs) 
{
    struct bio *bio;
    struct block_device *bd;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags;

    bio = (struct bio *)PT_REGS_PARM1(regs);
    bpf_core_read(&bd, sizeof(bd), &bio->bi_bdev);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &bd->bd_disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);
    bpf_core_read(&cmd_flags, sizeof(cmd_flags), &bio->bi_opf);

    if (major == 0) {
        log_event(STAGE_BIO, PERIOD_END, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_bio(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    void *delete_map = NULL; 
    struct io_counter *counterp = bpf_map_lookup_elem(&bio_map, &bio);
    if (!counterp) {
        return 0;
    }

    delete_map = &bio_map; 

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    };

    struct stage_data *curr_data;
    curr_data = bpf_map_lookup_elem(&bio_res, &key);
    if (curr_data == NULL && duration > DURATION_THRESHOLD) {
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 1,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&bio_res, &key, &new_data, 0); 
    } else if (curr_data == NULL) {
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&bio_res, &key, &new_data, 0); 
    } else {
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, duration);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&bio_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) { 
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&bio_res_2, &curr_start_range, &new_data, 0);
    } else {
        if (key < MAP_SIZE && curr_data_time_range->count[key] > 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], -1);
        }
    }
    bpf_map_delete_elem(delete_map, &bio);
    return 0; 
}

// start get_tag 
SEC("kprobe/blk_mq_get_tag") 
int kprobe_blk_mq_get_tag(struct pt_regs *regs) 
{
    u64 tagkey = bpf_get_current_task();
    u64 value = (u64)PT_REGS_PARM1(regs);
    (void)bpf_map_update_elem(&tag_args, &tagkey, &value, BPF_ANY);

    struct blk_mq_alloc_data *bd;
    struct request_queue *q;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags = 0;

    bd = (struct blk_mq_alloc_data *)value;
    bpf_core_read(&q, sizeof(q), &bd->q);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &q->disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);

    if (major == 0) {
        log_event(STAGE_GET_TAG, PERIOD_START, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_get_tag(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    struct io_counter *counterp, zero = {}; 
    init_io_counter(&zero, major, first_minor);
    counterp = bpf_map_lookup_elem(&tag_map, &tagkey);
    if (counterp) {
        return 0;
    }
    long err = bpf_map_update_elem(&tag_map, &tagkey, &zero, BPF_NOEXIST); 
    if (err) {
        log_event(STAGE_GET_TAG, PERIOD_START, ERROR_UPDATE_FAIL);
        return 0;
    }

    u64 curr_start_range = zero.start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    };

    struct stage_data *curr_data; 
    curr_data = bpf_map_lookup_elem(&tag_res, &key);
    if (!curr_data) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 0,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&tag_res, &key, &new_data, 0);
    } else { 
        update_curr_data_in_start(curr_data, &params);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&tag_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) {
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&tag_res_2, &curr_start_range, &new_data, 0); 
    } else {
        if (key < MAP_SIZE && key >= 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }
    return 0; 
}

// finish get_tag 
SEC("kretprobe/blk_mq_get_tag") 
int kretprobe_blk_mq_get_tag(struct pt_regs *regs) 
{
    u64 tagkey = bpf_get_current_task();
    u64 *tagargs = (u64 *)bpf_map_lookup_elem(&tag_args, &tagkey);
    if (tagargs == NULL) {
        bpf_map_delete_elem(&tag_args, &tagkey);
        return 0;
    }

    struct blk_mq_alloc_data *bd;
    struct request_queue *q;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags = 0;

    bd = (struct blk_mq_alloc_data *)*tagargs;
    bpf_core_read(&q, sizeof(q), &bd->q);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &q->disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);

    if (major == 0) {
        log_event(STAGE_GET_TAG, PERIOD_END, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_get_tag(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    struct io_counter *counterp = bpf_map_lookup_elem(&tag_map, &tagkey);
    if (!counterp) {
        return 0;
    }

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    };

    struct stage_data *curr_data; 
    curr_data = bpf_map_lookup_elem(&tag_res, &key); 
    if (curr_data == NULL && duration > DURATION_THRESHOLD) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 1,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&tag_res, &key, &new_data, 0); 
    } else if (curr_data == NULL) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&tag_res, &key, &new_data, 0); 
    } else {
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, duration);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&tag_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) { 
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&tag_res_2, &curr_start_range, &new_data, 0);
    } else {
        if (key < MAP_SIZE && curr_data_time_range->count[key] > 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], -1);
        }
    }

    bpf_map_delete_elem(&tag_map, &tagkey); 
    bpf_map_delete_elem(&tag_args, &tagkey);
    return 0; 
}

// start wbt
SEC("kprobe/wbt_wait") 
int kprobe_wbt_wait(struct pt_regs *regs) 
{
    u64 wbtkey = bpf_get_current_task();
    u64 value = (u64)PT_REGS_PARM2(regs);
    (void)bpf_map_update_elem(&wbt_args, &wbtkey, &value, BPF_ANY);
    
    struct bio *bio;
    struct block_device *bd;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags;

    bio = (struct bio *)value;
    bpf_core_read(&bd, sizeof(bd), &bio->bi_bdev);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &bd->bd_disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);
    bpf_core_read(&cmd_flags, sizeof(cmd_flags), &bio->bi_opf);

    if (major == 0) {
        log_event(STAGE_WBT, PERIOD_START, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_wbt(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    struct io_counter *counterp, zero = {}; 
    init_io_counter(&zero, major, first_minor);
    counterp = bpf_map_lookup_elem(&wbt_map, &wbtkey);
    if (counterp) {
        return 0;
    }
    long err = bpf_map_update_elem(&wbt_map, &wbtkey, &zero, BPF_NOEXIST); 
    if (err) {
        log_event(STAGE_WBT, PERIOD_START, ERROR_UPDATE_FAIL);
        return 0;
    }

    u64 curr_start_range = zero.start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    }; 

    struct stage_data *curr_data; 
    curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    if (!curr_data) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 0,
            .finish_over_time = 0,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&wbt_res, &key, &new_data, 0);
    } else {
        update_curr_data_in_start(curr_data, &params);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&wbt_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) {
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&wbt_res_2, &curr_start_range, &new_data, 0); 
    } else {
        if (key < MAP_SIZE && key >= 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }
    return 0; 
}

// finish wbt
SEC("kretprobe/wbt_wait") 
int kretprobe_wbt_wait(struct pt_regs *regs) 
{
    u64 wbtkey = bpf_get_current_task();
    u64 *wbtargs = (u64 *)bpf_map_lookup_elem(&wbt_args, &wbtkey);
    if (wbtargs == NULL) {
        bpf_map_delete_elem(&wbt_args, &wbtkey);
        return 0;
    }
    
    struct bio *bio;
    struct block_device *bd;
    struct gendisk *curr_rq_disk;
    int major, first_minor;
    unsigned int cmd_flags;

    bio = (struct bio *)(*wbtargs);
    bpf_core_read(&bd, sizeof(bd), &bio->bi_bdev);
    bpf_core_read(&curr_rq_disk, sizeof(curr_rq_disk), &bd->bd_disk);
    bpf_core_read(&major, sizeof(major), &curr_rq_disk->major);
    bpf_core_read(&first_minor, sizeof(first_minor), &curr_rq_disk->first_minor);
    bpf_core_read(&cmd_flags, sizeof(cmd_flags), &bio->bi_opf);

    if (major == 0) {
        log_event(STAGE_WBT, PERIOD_END, ERROR_MAJOR_ZERO);
        return 0;
    }

    u32 key = find_matching_key_wbt(major, first_minor);
    if (key >= MAP_SIZE) {
        return 0;
    }

    struct io_counter *counterp = bpf_map_lookup_elem(&wbt_map, &wbtkey); 
    if (!counterp) {
        return 0;
    }

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .curr_start_range = curr_start_range,
    }; 

    struct stage_data *curr_data; 
    curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    if (curr_data == NULL && duration > DURATION_THRESHOLD) { 
       struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 1,
            .duration = 0,
            .major = major,
            .first_minor = first_minor,
            .io_type = "",
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&wbt_res, &key, &new_data, 0); 
    } else if (curr_data == NULL) { 
        struct stage_data new_data = {
            .start_count = 1,
            .finish_count = 1,
            .finish_over_time = 0,
            .duration = 0,
            .io_type = "",
            .major = major,
            .first_minor = first_minor,
        };
        blk_fill_rwbs(new_data.io_type, cmd_flags);
        bpf_map_update_elem(&wbt_res, &key, &new_data, 0); 
    } else {
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, duration);
    }

    struct time_range_io_count *curr_data_time_range;
    curr_data_time_range = bpf_map_lookup_elem(&wbt_res_2, &curr_start_range);
    if (curr_data_time_range == NULL) { 
        struct time_range_io_count new_data = { .count = {0} };
        bpf_map_update_elem(&wbt_res_2, &curr_start_range, &new_data, 0);
    } else {
        if (key < MAP_SIZE && curr_data_time_range->count[key] > 0) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], -1);
        }
    }
    bpf_map_delete_elem(&wbt_map, &wbtkey);
    bpf_map_delete_elem(&wbt_args, &wbtkey);
    return 0; 
}

char _license[] SEC("license") = "GPL";
