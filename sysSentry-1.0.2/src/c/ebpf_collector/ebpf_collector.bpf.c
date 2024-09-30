/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: ebpf collector program
 * Author: Zhang Nan
 * Create: 2024-09-27
 */
#define KBUILD_MODNAME "foo" 

#include <linux/ptrace.h> 
#include <linux/version.h> 
#include <linux/blkdev.h> 
#include <uapi/linux/bpf.h> 
#include <uapi/linux/in6.h> 
#include <uapi/linux/stddef.h>
#include <linux/kdev_t.h>
#include <linux/blk-mq.h>
#include <linux/spinlock_types.h>
#include <linux/blk_types.h>
#include <sys/sysmacros.h>
#include "bpf_helpers.h" 
#include "ebpf_collector.h"

#define _(P) ({typeof(P) val; bpf_probe_read(&val, sizeof(val), &P); val;})

struct bpf_map_def SEC("maps") blk_map = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct io_counter), 
   .max_entries = 10000, 
};

struct bpf_map_def SEC("maps") blk_res = { 
   .type = BPF_MAP_TYPE_ARRAY, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct stage_data), 
   .max_entries = 128, 
};

struct bpf_map_def SEC("maps") bio_map = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct io_counter), 
   .max_entries = 10000, 
};

struct bpf_map_def SEC("maps") bio_res = { 
   .type = BPF_MAP_TYPE_ARRAY, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct stage_data), 
   .max_entries = 128, 
};

struct bpf_map_def SEC("maps") wbt_map = { 
   .type = BPF_MAP_TYPE_HASH,
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct io_counter), 
   .max_entries = 10000, 
};

struct bpf_map_def SEC("maps") wbt_res = { 
   .type = BPF_MAP_TYPE_ARRAY, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct stage_data), 
   .max_entries = 128, 
};

struct bpf_map_def SEC("maps") wbt_args = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(u64), 
   .max_entries = 1000, 
};

struct bpf_map_def SEC("maps") tag_map = { 
   .type = BPF_MAP_TYPE_HASH,
   .key_size = sizeof(u32),
   .value_size = sizeof(struct io_counter), 
   .max_entries = 10000, 
};

struct bpf_map_def SEC("maps") tag_res = { 
   .type = BPF_MAP_TYPE_ARRAY, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(struct stage_data), 
   .max_entries = 128, 
};

struct bpf_map_def SEC("maps") tag_args = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u32), 
   .value_size = sizeof(u64), 
   .max_entries = 1000, 
};

struct blk_mq_alloc_data {
    /* input parameter */
    struct request_queue *q;
    blk_mq_req_flags_t flags;
    unsigned int shallow_depth;

    /* input & output parameter */
    struct blk_mq_ctx *ctx;
    struct blk_mq_hw_ctx *hctx;
};

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

void update_new_data_in_start(struct stage_data *new_data, struct update_params *params) {
    blk_fill_rwbs(new_data->io_type, params->cmd_flags);
    if (new_data->bucket[params->update_bucket].start_range == params->curr_start_range){
        new_data->bucket[params->update_bucket].io_count += 1;
    } else {
        new_data->bucket[MAX_BUCKETS].io_count += new_data->bucket[params->update_bucket].io_count;
        new_data->bucket[params->update_bucket].io_count = 1;
        new_data->bucket[params->update_bucket].start_range = params->curr_start_range;
    }
}

void update_curr_data_in_start(struct stage_data *curr_data, struct update_params *params) {
    if (curr_data && params) {
        curr_data->start_count += 1;
        curr_data->major = params->major;
        curr_data->first_minor = params->first_minor;
        blk_fill_rwbs(curr_data->io_type, params->cmd_flags);
        if (curr_data->bucket[params->update_bucket].start_range == params->curr_start_range) {
            curr_data->bucket[params->update_bucket].io_count += 1;
        } else {
            curr_data->bucket[MAX_BUCKETS].io_count += curr_data->bucket[params->update_bucket].io_count;
            curr_data->bucket[params->update_bucket].io_count = 1;
        }
        curr_data->bucket[params->update_bucket].start_range = params->curr_start_range;
    }
}

void update_new_data_in_finish(struct stage_data *new_data, struct update_params *params) {
    blk_fill_rwbs(new_data->io_type, params->cmd_flags);
    if (new_data->bucket[params->update_bucket].start_range == params->curr_start_range){
        new_data->bucket[params->update_bucket].io_count = (new_data->bucket[params->update_bucket].io_count > 1) ? new_data->bucket[params->update_bucket].io_count - 1 : 0;
    } else {
        new_data->bucket[MAX_BUCKETS].io_count = (new_data->bucket[MAX_BUCKETS].io_count > 1) ? new_data->bucket[MAX_BUCKETS].io_count - 1 : 0;
    }
}

void update_curr_data_in_finish(struct stage_data *curr_data, struct update_params *params, u64 duration) {
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


u32 find_matching_tag_1_keys(int major, int minor) {
    u32 key = 0;
    struct stage_data *curr_data = bpf_map_lookup_elem(&tag_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 1;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&tag_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 2;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&tag_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }

    return MAP_SIZE + 1; 
}

u32 find_matching_tag_2_keys(int major, int minor) {
    u32 key = 3;
    struct stage_data *curr_data = bpf_map_lookup_elem(&tag_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 4;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&tag_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 5;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&tag_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_tag_3_keys(int major, int minor) {
    u32 key = 6;
    struct stage_data *curr_data = bpf_map_lookup_elem(&tag_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 7;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&tag_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 8;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&tag_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_tag_4_keys(int major, int minor) {
    u32 key = 9;
    struct stage_data *curr_data = bpf_map_lookup_elem(&tag_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 10;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&tag_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 11;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&tag_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_tag_5_keys(int major, int minor) {
    u32 key = 12;
    struct stage_data *curr_data = bpf_map_lookup_elem(&tag_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 13;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&tag_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 14;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&tag_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_blk_1_keys(int major, int minor) {
    u32 key = 0;
    struct stage_data *curr_data = bpf_map_lookup_elem(&blk_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 1;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&blk_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 2;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&blk_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }

    return MAP_SIZE + 1; 
}

u32 find_matching_blk_2_keys(int major, int minor) {
    u32 key = 3;
    struct stage_data *curr_data = bpf_map_lookup_elem(&blk_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 4;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&blk_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 5;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&blk_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_blk_3_keys(int major, int minor) {
    u32 key = 6;
    struct stage_data *curr_data = bpf_map_lookup_elem(&blk_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 7;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&blk_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 8;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&blk_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_blk_4_keys(int major, int minor) {
    u32 key = 9;
    struct stage_data *curr_data = bpf_map_lookup_elem(&blk_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 10;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&blk_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 11;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&blk_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_blk_5_keys(int major, int minor) {
    u32 key = 12;
    struct stage_data *curr_data = bpf_map_lookup_elem(&blk_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 13;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&blk_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 14;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&blk_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_bio_1_keys(int major, int minor) {
    u32 key = 0;
    struct stage_data *curr_data = bpf_map_lookup_elem(&bio_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 1;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&bio_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 2;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&bio_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }

    return MAP_SIZE + 1; 
}

u32 find_matching_bio_2_keys(int major, int minor) {
    u32 key = 3;
    struct stage_data *curr_data = bpf_map_lookup_elem(&bio_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 4;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&bio_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 5;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&bio_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_bio_3_keys(int major, int minor) {
    u32 key = 6;
    struct stage_data *curr_data = bpf_map_lookup_elem(&bio_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 7;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&bio_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 8;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&bio_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_bio_4_keys(int major, int minor) {
    u32 key = 9;
    struct stage_data *curr_data = bpf_map_lookup_elem(&bio_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 10;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&bio_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 11;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&bio_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_bio_5_keys(int major, int minor) {
    u32 key = 12;
    struct stage_data *curr_data = bpf_map_lookup_elem(&bio_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 13;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&bio_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 14;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&bio_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_wbt_1_keys(int major, int minor) {
    u32 key = 0;
    struct stage_data *curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 1;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&wbt_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 2;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&wbt_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }

    return MAP_SIZE + 1; 
}

u32 find_matching_wbt_2_keys(int major, int minor) {
    u32 key = 3;
    struct stage_data *curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 4;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&wbt_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 5;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&wbt_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_wbt_3_keys(int major, int minor) {
    u32 key = 6;
    struct stage_data *curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 7;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&wbt_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 8;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&wbt_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_wbt_4_keys(int major, int minor) {
    u32 key = 9;
    struct stage_data *curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 10;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&wbt_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 11;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&wbt_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

u32 find_matching_wbt_5_keys(int major, int minor) {
    u32 key = 12;
    struct stage_data *curr_data = bpf_map_lookup_elem(&wbt_res, &key);
    
    if (curr_data != NULL && curr_data->major == major && curr_data->first_minor == minor) {
        return key; 
    }

    u32 key_2 = 13;
    struct stage_data *curr_data_2 = bpf_map_lookup_elem(&wbt_res, &key_2);
    
    if (curr_data_2 != NULL && curr_data_2->major == major && curr_data_2->first_minor == minor) {
        return key_2; 
    }

    u32 key_3 = 14;
    struct stage_data *curr_data_3 = bpf_map_lookup_elem(&wbt_res, &key_3);
    
    if (curr_data_3 != NULL && curr_data_3->major == major && curr_data_3->first_minor == minor) {
        return key_3; 
    }
    
    return MAP_SIZE + 1; 
}

SEC("kprobe/blk_mq_start_request") 
int kprobe_blk_mq_start_request(struct pt_regs *regs) 
{
    struct request *rq = (struct request *)PT_REGS_PARM1(regs); 
    struct gendisk *curr_rq_disk = _(rq->rq_disk);
    int major = _(curr_rq_disk->major);
    int first_minor = _(curr_rq_disk->first_minor);
    unsigned int cmd_flags =  _(rq->cmd_flags);
    
    struct io_counter *counterp, zero = {}; 

    u32 key = find_matching_blk_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_blk_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_blk_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_blk_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_blk_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    } 
    
    init_io_counter(&zero, major, first_minor);

    counterp = bpf_map_lookup_elem(&blk_map, &rq); 
    if (counterp || major == 0) 
        return 0;
    long err = bpf_map_update_elem(&blk_map, &rq, &zero, BPF_NOEXIST); 
    if (err) 
        return 0;

    u64 curr_start_range = zero.start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        }; 
        update_new_data_in_start(&new_data, &params);
        bpf_map_update_elem(&blk_res, &key, &new_data, 0);
    } else {
        update_curr_data_in_start(curr_data, &params);
    }

    return 0; 
}

SEC("kprobe/blk_mq_free_request") 
int kprobe_blk_mq_free_request(struct pt_regs *regs) 
{
    struct request *rq = (struct request *)PT_REGS_PARM1(regs);
    struct gendisk *curr_rq_disk = _(rq->rq_disk);
    int major = _(curr_rq_disk->major);
    int first_minor = _(curr_rq_disk->first_minor);
    unsigned int cmd_flags =  _(rq->cmd_flags); 

    struct io_counter *counterp;
    u32 key = find_matching_blk_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_blk_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_blk_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_blk_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_blk_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    } 
    
    counterp = bpf_map_lookup_elem(&blk_map, &rq);

    if (!counterp) { 
        return 0;
    }

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
        bpf_map_update_elem(&blk_res, &key, &new_data, 0); 
    } else {
        if (curr_data->bucket[update_bucket].start_range == curr_start_range) {
            curr_data->bucket[update_bucket].io_count = (curr_data->bucket[update_bucket].io_count > 1) ? curr_data->bucket[update_bucket].io_count - 1 : 0;
        } else {
            curr_data->bucket[MAX_BUCKETS].io_count = (curr_data->bucket[MAX_BUCKETS].io_count > 1) ? curr_data->bucket[MAX_BUCKETS].io_count - 1 : 0;

        }
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, &duration);
    }

    bpf_map_delete_elem(&blk_map, &rq); 
    return 0;
}

SEC("kprobe/blk_mq_make_request") 
int kprobe_blk_mq_make_request(struct pt_regs *regs)
{
    struct bio *bio = (struct bio *)PT_REGS_PARM2(regs);
    struct gendisk *curr_rq_disk = _(bio->bi_disk);
    int major = _(curr_rq_disk->major);
    int first_minor = _(curr_rq_disk->first_minor);
    unsigned int cmd_flags =  _(bio->bi_opf);

    struct io_counter *counterp, zero = {};
    u32 key = find_matching_bio_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_bio_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_bio_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_bio_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_bio_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    } 

    init_io_counter(&zero, major, first_minor);

    counterp = bpf_map_lookup_elem(&bio_map, &bio); 
    if (counterp || major == 0) 
        return 0;

    long err = bpf_map_update_elem(&bio_map, &bio, &zero, BPF_NOEXIST); 
    if (err && err != -EEXIST) 
        return 0;

    u64 curr_start_range = zero.start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_start(&new_data, &params);
        bpf_map_update_elem(&bio_res, &key, &new_data, 0);
    } else { 
        update_curr_data_in_start(curr_data, &params);
    }

    return 0; 
}

SEC("kprobe/bio_endio") 
int kprobe_bio_endio(struct pt_regs *regs) 
{
    struct bio *bio = (struct bio *)PT_REGS_PARM1(regs);
    struct gendisk *curr_rq_disk = _(bio->bi_disk);
    int major = _(curr_rq_disk->major);
    int first_minor = _(curr_rq_disk->first_minor);
    unsigned int cmd_flags =  _(bio->bi_opf);

    struct io_counter *counterp; 
    void *delete_map = NULL; 
    u32 key = find_matching_bio_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_bio_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_bio_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_bio_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_bio_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    }

    counterp = bpf_map_lookup_elem(&bio_map, &bio);

    if (!counterp) { 
        return 0;
    }

    delete_map = &bio_map; 

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
        bpf_map_update_elem(&bio_res, &key, &new_data, 0); 
    } else {
        if (curr_data->bucket[update_bucket].start_range == curr_start_range) {
            curr_data->bucket[update_bucket].io_count = (curr_data->bucket[update_bucket].io_count > 1) ? curr_data->bucket[update_bucket].io_count - 1 : 0;
        } else {
            curr_data->bucket[MAX_BUCKETS].io_count = (curr_data->bucket[MAX_BUCKETS].io_count > 1) ? curr_data->bucket[MAX_BUCKETS].io_count - 1 : 0;

        }
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, &duration);
    }

    bpf_map_delete_elem(delete_map, &bio); 
    return 0; 
}

SEC("kprobe/wbt_wait") 
int kprobe_wbt_wait(struct pt_regs *regs) 
{
    u64 wbtkey = bpf_get_current_task();
    u64 value = (u64)PT_REGS_PARM2(regs);
    (void)bpf_map_update_elem(&wbt_args, &wbtkey, &value, BPF_ANY);
    struct bio *bio = (struct bio *)value;
    struct gendisk *curr_rq_disk = _(bio->bi_disk);
    int major = _(curr_rq_disk->major);
    int first_minor = _(curr_rq_disk->first_minor);
    unsigned int cmd_flags =  _(bio->bi_opf);

    struct io_counter *counterp, zero = {}; 
    u32 key = find_matching_wbt_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_wbt_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_wbt_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_wbt_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_wbt_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    }

    init_io_counter(&zero, major, first_minor);

    counterp = bpf_map_lookup_elem(&wbt_map, &wbtkey);

    if (counterp || major == 0) 
        return 0;
    long err = bpf_map_update_elem(&wbt_map, &wbtkey, &zero, BPF_NOEXIST); 
    if (err) 
        return 0;

    u64 curr_start_range = zero.start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_start(&new_data, &params);
        bpf_map_update_elem(&wbt_res, &key, &new_data, 0);
    } else {
        update_curr_data_in_start(curr_data, &params);
    }

    return 0; 
}

SEC("kretprobe/wbt_wait") 
int kretprobe_wbt_wait(struct pt_regs *regs) 
{
    struct bio *bio = NULL;
    u64 *wbtargs = NULL;
    u64 wbtkey = bpf_get_current_task();
    wbtargs = (u64 *)bpf_map_lookup_elem(&wbt_args, &wbtkey);
    if (wbtargs == NULL) {
        bpf_map_delete_elem(&wbt_args, &wbtkey);
        return 0;
    }
    bio = (struct bio *)(*wbtargs);
    struct gendisk *curr_rq_disk = _(bio->bi_disk);
    int major = _(curr_rq_disk->major);
    int first_minor = _(curr_rq_disk->first_minor);
    unsigned int cmd_flags =  _(bio->bi_opf);
    
    struct io_counter *counterp; 
    u32 key = find_matching_wbt_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_wbt_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_wbt_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_wbt_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_wbt_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    }

    counterp = bpf_map_lookup_elem(&wbt_map, &wbtkey); 

    if (!counterp)
        return 0;

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
        bpf_map_update_elem(&wbt_res, &key, &new_data, 0); 
    } else {
        if (curr_data->bucket[update_bucket].start_range == curr_start_range) {
            curr_data->bucket[update_bucket].io_count = (curr_data->bucket[update_bucket].io_count > 1) ? curr_data->bucket[update_bucket].io_count - 1 : 0;
        } else {
            curr_data->bucket[MAX_BUCKETS].io_count = (curr_data->bucket[MAX_BUCKETS].io_count > 1) ? curr_data->bucket[MAX_BUCKETS].io_count - 1 : 0;

        }
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, &duration);
    }

    bpf_map_delete_elem(&wbt_map, &wbtkey);
    bpf_map_delete_elem(&wbt_args, &wbtkey);
    return 0; 
}

SEC("kprobe/blk_mq_get_tag") 
int kprobe_blk_mq_get_tag(struct pt_regs *regs) 
{ 
    u64 tagkey = bpf_get_current_task();
    u64 value = (u64)PT_REGS_PARM1(regs);
    (void)bpf_map_update_elem(&tag_args, &tagkey, &value, BPF_ANY);
    struct blk_mq_alloc_data *bd= (struct blk_mq_alloc_data *)value;
    struct request_queue *q = _(bd->q);
    struct backing_dev_info	*backing_dev_info = _(q->backing_dev_info);
    struct device *owner = _(backing_dev_info->owner);
    dev_t devt = _(owner->devt);
    int major = MAJOR(devt);
    int first_minor = MINOR(devt);
    unsigned int cmd_flags =  0;

    struct io_counter *counterp, zero = {}; 
    u32 key = find_matching_tag_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_tag_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_tag_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_tag_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_tag_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    }

    init_io_counter(&zero, major, first_minor); 

    counterp = bpf_map_lookup_elem(&tag_map, &tagkey); 
    if (counterp || major == 0) 
        return 0;
    long err = bpf_map_update_elem(&tag_map, &tagkey, &zero, BPF_NOEXIST); 
    if (err) 
        return 0;
    
    u64 curr_start_range = zero.start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_start(&new_data, &params);
        bpf_map_update_elem(&tag_res, &key, &new_data, 0);
    } else { 
        update_curr_data_in_start(curr_data, &params);
    }

    return 0; 
}

SEC("kretprobe/blk_mq_get_tag") 
int kretprobe_blk_mq_get_tag(struct pt_regs *regs) 
{
    u64 tagkey = bpf_get_current_task();
    u64 *tagargs = NULL;
    struct blk_mq_alloc_data *bd = NULL;

    tagargs = (u64 *)bpf_map_lookup_elem(&tag_args, &tagkey);
    if (tagargs == NULL) {
        bpf_map_delete_elem(&tag_args, &tagkey);
    	return 0;
    }
    bd = (struct blk_mq_alloc_data *)(*tagargs);
    struct request_queue *q = _(bd->q);
    struct backing_dev_info	*backing_dev_info = _(q->backing_dev_info);
    struct device *owner = _(backing_dev_info->owner);
    dev_t devt = _(owner->devt);
    int major = MAJOR(devt);
    int first_minor = MINOR(devt);
    unsigned int cmd_flags =  0;

    struct io_counter *counterp; 
    u32 key = find_matching_tag_1_keys(major, first_minor);
    if (key >= MAP_SIZE){
        key = find_matching_tag_2_keys(major, first_minor);
        if (key >= MAP_SIZE){
            key = find_matching_tag_3_keys(major, first_minor);
            if (key >= MAP_SIZE){
                key = find_matching_tag_4_keys(major, first_minor);
                if (key >= MAP_SIZE){
                    key = find_matching_tag_5_keys(major, first_minor);
                    if (key >= MAP_SIZE){
                        return 0;
                    }
                }
            }
        }
    }

    counterp = bpf_map_lookup_elem(&tag_map, &tagkey);

    if (!counterp) 
        return 0;

    u64 duration = bpf_ktime_get_ns() - counterp->start_time; 
    u64 curr_start_range = counterp->start_time / THRESHOLD / MAX_BUCKETS;
    u64 update_bucket = curr_start_range % MAX_BUCKETS;

    struct update_params params = {
        .major = major,
        .first_minor = first_minor,
        .cmd_flags = cmd_flags,
        .update_bucket = update_bucket,
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
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
            .bucket = {
                [0] = {.start_range = 0, .io_count = 0},
                [1] = {.start_range = 0, .io_count = 0},
            },
        };
        update_new_data_in_finish(&new_data, &params);
        bpf_map_update_elem(&tag_res, &key, &new_data, 0); 
    } else {
        if (curr_data->bucket[update_bucket].start_range == curr_start_range) {
            curr_data->bucket[update_bucket].io_count = (curr_data->bucket[update_bucket].io_count > 1) ? curr_data->bucket[update_bucket].io_count - 1 : 0;
        } else {
            curr_data->bucket[MAX_BUCKETS].io_count = (curr_data->bucket[MAX_BUCKETS].io_count > 1) ? curr_data->bucket[MAX_BUCKETS].io_count - 1 : 0;

        }
        curr_data->duration += duration; 
        update_curr_data_in_finish(curr_data, &params, &duration);
    }
    bpf_map_delete_elem(&tag_map, &tagkey); 
    bpf_map_delete_elem(&tag_args, &tagkey);
    return 0; 
}

char LICENSE[] SEC("license") = "Dual BSD/GPL"; 
u32 _version SEC("version") = LINUX_VERSION_CODE; 
