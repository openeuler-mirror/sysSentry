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

#define VERSION_KY_V2401    1
#define VERSION_KY_V2101    2

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

struct bpf_map_def SEC("maps") blk_res_2 = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u64), 
   .value_size = sizeof(struct time_range_io_count), 
   .max_entries = MAX_IO_TIME, 
};

struct bpf_map_def SEC("maps") bio_res_2 = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u64), 
   .value_size = sizeof(struct time_range_io_count), 
   .max_entries = MAX_IO_TIME, 
};

struct bpf_map_def SEC("maps") wbt_res_2 = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u64), 
   .value_size = sizeof(struct time_range_io_count), 
   .max_entries = MAX_IO_TIME, 
};

struct bpf_map_def SEC("maps") tag_res_2 = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(u64), 
   .value_size = sizeof(struct time_range_io_count), 
   .max_entries = MAX_IO_TIME, 
};

struct bpf_map_def SEC("maps") version_res = { 
   .type = BPF_MAP_TYPE_HASH, 
   .key_size = sizeof(32), 
   .value_size = sizeof(int), 
   .max_entries = MAX_IO_TIME, 
};

// 麒麟v2101平台
struct request_kylin_v2101 {
    struct request_queue *q;
    struct blk_mq_ctx *mq_ctx;

    int cpu;
    unsigned int cmd_flags;        /* op and common flags */
    req_flags_t rq_flags;

    int internal_tag;

    /* the following two fields are internal, NEVER access directly */
    unsigned int __data_len;    /* total data len */
    int tag;
    sector_t __sector;        /* sector cursor */

    struct bio *bio;
    struct bio *biotail;

    struct list_head queuelist;

    /*
     * The hash is used inside the scheduler, and killed once the
     * request reaches the dispatch list. The ipi_list is only used
     * to queue the request for softirq completion, which is long
     * after the request has been unhashed (and even removed from
     * the dispatch list).
     */
    union {
        struct hlist_node hash;    /* merge hash */
        struct list_head ipi_list;
    };

    struct hlist_node front_hash; /* front merge hash */

    /*
     * The rb_node is only used inside the io scheduler, requests
     * are pruned when moved to the dispatch queue. So let the
     * completion_data share space with the rb_node.
     */
    union {
        struct rb_node rb_node;    /* sort/lookup */
        struct bio_vec special_vec;
        void *completion_data;
        int error_count; /* for legacy drivers, don't use */
    };

    /*
     * Three pointers are available for the IO schedulers, if they need
     * more they have to dynamically allocate it.  Flush requests are
     * never put on the IO scheduler. So let the flush fields share
     * space with the elevator data.
     */
    union {
        struct {
            struct io_cq        *icq;
            void            *priv[2];
        } elv;

        struct {
            unsigned int        seq;
            struct list_head    list;
            rq_end_io_fn        *saved_end_io;
        } flush;
    };

    struct gendisk *rq_disk;
    struct hd_struct *part;
    /* Time that I/O was submitted to the kernel. */
    u64 start_time_ns;
    /* Time that I/O was submitted to the device. */
    u64 io_start_time_ns;

#ifdef CONFIG_BLK_WBT
    unsigned short wbt_flags;
#endif
#ifdef CONFIG_BLK_DEV_THROTTLING_LOW
    unsigned short throtl_size;
#endif

    /*
     * Number of scatter-gather DMA addr+len pairs after
     * physical address coalescing is performed.
     */
    unsigned short nr_phys_segments;

#if defined(CONFIG_BLK_DEV_INTEGRITY)
    unsigned short nr_integrity_segments;
#endif

    unsigned short write_hint;
    unsigned short ioprio;

    void *special;        /* opaque pointer available for LLD use */

    unsigned int extra_len;    /* length of alignment and padding */

    enum mq_rq_state state;
    refcount_t ref;

    unsigned int timeout;

    /* access through blk_rq_set_deadline, blk_rq_deadline */
    unsigned long __deadline;

    struct list_head timeout_list;

    union {
        struct __call_single_data csd;
        u64 fifo_time;
    };

    /*
     * completion callback.
     */
    rq_end_io_fn *end_io;
    void *end_io_data;

    /* for bidi */
    struct request_kylin_v2101 *next_rq;

#ifdef CONFIG_BLK_CGROUP
    struct request_list *rl;        /* rl this rq is alloced from */
#endif
    KABI_RESERVE(1);
    KABI_RESERVE(2);
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

// 麒麟v2401平台
struct request_kylin_v2401 {
    struct request_queue *q;
    struct blk_mq_ctx *mq_ctx;
    struct blk_mq_hw_ctx *mq_hctx;

    unsigned int cmd_flags;     /* op and common flags */
    req_flags_t rq_flags;

    int internal_tag;

    /* the following two fields are internal, NEVER access directly */
    unsigned int __data_len;    /* total data len */
    int tag;
    sector_t __sector;      /* sector cursor */

    struct bio *bio;
    struct bio *biotail;

    struct list_head queuelist;

    /*
     * The hash is used inside the scheduler, and killed once the
     * request reaches the dispatch list. The ipi_list is only used
     * to queue the request for softirq completion, which is long
     * after the request has been unhashed (and even removed from
     * the dispatch list).
     */
    union {
        struct hlist_node hash; /* merge hash */
        struct list_head ipi_list;
    };

    struct hlist_node front_hash; /* front merge hash */

    /*
     * The rb_node is only used inside the io scheduler, requests
     * are pruned when moved to the dispatch queue. So let the
     * completion_data share space with the rb_node.
     */
    union {
        struct rb_node rb_node; /* sort/lookup */
        struct bio_vec special_vec;
        void *completion_data;
        int error_count; /* for legacy drivers, don't use */
    };

    /*
     * Three pointers are available for the IO schedulers, if they need
     * more they have to dynamically allocate it.  Flush requests are
     * never put on the IO scheduler. So let the flush fields share
     * space with the elevator data.
     */
    union {
        struct {
            struct io_cq        *icq;
            void            *priv[2];
        } elv;

        struct {
            unsigned int        seq;
            struct list_head    list;
            rq_end_io_fn        *saved_end_io;
        } flush;
    };

    struct gendisk *rq_disk;
    struct hd_struct *part;
#ifdef CONFIG_BLK_RQ_ALLOC_TIME
    /* Time that the first bio started allocating this request. */
    u64 alloc_time_ns;
#endif
    /* Time that this request was allocated for this IO. */
    u64 start_time_ns;
    /* Time that I/O was submitted to the device. */
    u64 io_start_time_ns;

#ifdef CONFIG_BLK_WBT
    unsigned short wbt_flags;
#endif
#ifdef CONFIG_BLK_DEV_THROTTLING_LOW
    unsigned short throtl_size;
#endif

    /*
     * Number of scatter-gather DMA addr+len pairs after
     * physical address coalescing is performed.
     */
    unsigned short nr_phys_segments;

#if defined(CONFIG_BLK_DEV_INTEGRITY)
    unsigned short nr_integrity_segments;
#endif

    unsigned short write_hint;
    unsigned short ioprio;

    void *special;      /* opaque pointer available for LLD use */

    unsigned int extra_len; /* length of alignment and padding */

    enum mq_rq_state state;
    refcount_t ref;

    unsigned int timeout;

    /* access through blk_rq_set_deadline, blk_rq_deadline */
    unsigned long __deadline;

    union {
        struct __call_single_data csd;
        u64 fifo_time;
    };

    /*
     * completion callback.
     */
    rq_end_io_fn *end_io;
    void *end_io_data;

    /* for bidi */
    struct request_kylin_v2401 *next_rq;
    KABI_RESERVE(1);
    KABI_RESERVE(2);
};

struct request_queue_kylin_v2401 {
	/*
	 * Together with queue_head for cacheline sharing
	 */
	struct list_head	queue_head;
	struct request		*last_merge;
	struct elevator_queue	*elevator;

	struct blk_queue_stats	*stats;
	struct rq_qos		*rq_qos;

	make_request_fn		*make_request_fn;
	poll_q_fn		*poll_fn;
	dma_drain_needed_fn	*dma_drain_needed;

	const struct blk_mq_ops	*mq_ops;

	/* sw queues */
	struct blk_mq_ctx __percpu	*queue_ctx;
	unsigned int		nr_queues;

	unsigned int		queue_depth;

	/* hw dispatch queues */
	struct blk_mq_hw_ctx	**queue_hw_ctx;
	unsigned int		nr_hw_queues;

	struct backing_dev_info_kylin_v2401	*backing_dev_info;

	/*
	 * The queue owner gets to use this for whatever they like.
	 * ll_rw_blk doesn't touch it.
	 */
	void			*queuedata;

	/*
	 * various queue flags, see QUEUE_* below
	 */
	unsigned long		queue_flags;
	/*
	 * Number of contexts that have called blk_set_pm_only(). If this
	 * counter is above zero then only RQF_PM and RQF_PREEMPT requests are
	 * processed.
	 */
	atomic_t		pm_only;

	/*
	 * ida allocated id for this queue.  Used to index queues from
	 * ioctx.
	 */
	int			id;

	/*
	 * queue needs bounce pages for pages above this limit
	 */
	gfp_t			bounce_gfp;

	/*
	 * protects queue structures from reentrancy. ->__queue_lock should
	 * _never_ be used directly, it is queue private. always use
	 * ->queue_lock.
	 */
	spinlock_t		__queue_lock;
	spinlock_t		*queue_lock;

	/*
	 * queue kobject
	 */
	struct kobject kobj;

	/*
	 * mq queue kobject
	 */
	struct kobject *mq_kobj;

#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct blk_integrity integrity;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */

#ifdef CONFIG_PM
	struct device		*dev;
	int			rpm_status;
	unsigned int		nr_pending;
#endif

	/*
	 * queue settings
	 */
	unsigned long		nr_requests;	/* Max # of requests */

	unsigned int		dma_drain_size;
	void			*dma_drain_buffer;
	unsigned int		dma_pad_mask;
	unsigned int		dma_alignment;

	unsigned int		rq_timeout;
	int			poll_nsec;

	struct blk_stat_callback	*poll_cb;
	struct blk_rq_stat	poll_stat[BLK_MQ_POLL_STATS_BKTS];

	struct timer_list	timeout;
	struct work_struct	timeout_work;

	atomic_t		nr_active_requests_shared_sbitmap;

	struct list_head	icq_list;
#ifdef CONFIG_BLK_CGROUP
	DECLARE_BITMAP		(blkcg_pols, BLKCG_MAX_POLS);
	struct blkcg_gq		*root_blkg;
	struct list_head	blkg_list;
#endif

	struct queue_limits	limits;

	unsigned int		required_elevator_features;

#ifdef CONFIG_BLK_DEV_ZONED
	/*
	 * Zoned block device information for request dispatch control.
	 * nr_zones is the total number of zones of the device. This is always
	 * 0 for regular block devices. seq_zones_bitmap is a bitmap of nr_zones
	 * bits which indicates if a zone is conventional (bit clear) or
	 * sequential (bit set). seq_zones_wlock is a bitmap of nr_zones
	 * bits which indicates if a zone is write locked, that is, if a write
	 * request targeting the zone was dispatched. All three fields are
	 * initialized by the low level device driver (e.g. scsi/sd.c).
	 * Stacking drivers (device mappers) may or may not initialize
	 * these fields.
	 *
	 * Reads of this information must be protected with blk_queue_enter() /
	 * blk_queue_exit(). Modifying this information is only allowed while
	 * no requests are being processed. See also blk_mq_freeze_queue() and
	 * blk_mq_unfreeze_queue().
	 */
	unsigned int		nr_zones;
	unsigned long		*seq_zones_bitmap;
	unsigned long		*seq_zones_wlock;
#endif /* CONFIG_BLK_DEV_ZONED */

	/*
	 * sg stuff
	 */
	unsigned int		sg_timeout;
	unsigned int		sg_reserved_size;
	int			node;
#ifdef CONFIG_BLK_DEV_IO_TRACE
	struct blk_trace __rcu	*blk_trace;
	struct mutex		blk_trace_mutex;
#endif
	/*
	 * for flush operations
	 */
	struct blk_flush_queue	*fq;

	struct list_head	requeue_list;
	spinlock_t		requeue_lock;
	struct delayed_work	requeue_work;

	struct mutex		sysfs_lock;

	/*
	 * for reusing dead hctx instance in case of updating
	 * nr_hw_queues
	 */
	struct list_head    unused_hctx_list;
	spinlock_t      unused_hctx_lock;

	int			mq_freeze_depth;

#if defined(CONFIG_BLK_DEV_BSG)
	struct bsg_class_device bsg_dev;
#endif

#ifdef CONFIG_BLK_DEV_THROTTLING
	/* Throttle data */
	struct throtl_data *td;
#endif
	struct rcu_head		rcu_head;
	wait_queue_head_t	mq_freeze_wq;
	/*
	 * Protect concurrent access to q_usage_counter by
	 * percpu_ref_kill() and percpu_ref_reinit().
	 */
	struct mutex            mq_freeze_lock;
	struct percpu_ref	q_usage_counter;
	struct list_head	all_q_node;

	struct blk_mq_tag_set	*tag_set;
	struct list_head	tag_set_list;
	struct bio_set		bio_split;

#ifdef CONFIG_BLK_DEBUG_FS
	struct dentry		*debugfs_dir;
	struct dentry		*sched_debugfs_dir;
#endif

	bool			mq_sysfs_init_done;

	size_t			cmd_size;
	void			*rq_alloc_data;

	struct work_struct	release_work;

#ifdef CONFIG_BLK_BIO_DISPATCH_ASYNC
	/* used when QUEUE_FLAG_DISPATCH_ASYNC is set */
	struct cpumask		dispatch_async_cpus;
	int __percpu		*last_dispatch_cpu;
#endif

#define BLK_MAX_WRITE_HINTS	5
	u64			write_hints[BLK_MAX_WRITE_HINTS];

	KABI_RESERVE(1);
	KABI_RESERVE(2);
	KABI_RESERVE(3);
	KABI_RESERVE(4);
};

struct backing_dev_info_kylin_v2401 {
	u64 id;
	struct rb_node rb_node; /* keyed by ->id */
	struct list_head bdi_list;
	unsigned long ra_pages;	/* max readahead in PAGE_SIZE units */
	unsigned long io_pages;	/* max allowed IO size */
	congested_fn *congested_fn; /* Function pointer if device is md/dm */
	void *congested_data;	/* Pointer to aux data for congested func */

	const char *name;

	struct kref refcnt;	/* Reference counter for the structure */
	unsigned int capabilities; /* Device capabilities */
	unsigned int min_ratio;
	unsigned int max_ratio, max_prop_frac;

	/*
	 * Sum of avg_write_bw of wbs with dirty inodes.  > 0 if there are
	 * any dirty wbs, which is depended upon by bdi_has_dirty().
	 */
	atomic_long_t tot_write_bandwidth;

	struct bdi_writeback wb;  /* the root writeback info for this bdi */
	struct list_head wb_list; /* list of all wbs */
#ifdef CONFIG_CGROUP_WRITEBACK
	struct radix_tree_root cgwb_tree; /* radix tree of active cgroup wbs */
	struct rb_root cgwb_congested_tree; /* their congested states */
	struct mutex cgwb_release_mutex;  /* protect shutdown of wb structs */
	struct rw_semaphore wb_switch_rwsem; /* no cgwb switch while syncing */
#else
	struct bdi_writeback_congested *wb_congested;
#endif
	wait_queue_head_t wb_waitq;

	union {
		struct rcu_device *rcu_dev;
		struct device *dev;
	};
	struct device *owner;

	struct timer_list laptop_mode_wb_timer;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debug_dir;
	struct dentry *debug_stats;
#endif

	KABI_RESERVE(1)
	KABI_RESERVE(2)
	KABI_RESERVE(3)
	KABI_RESERVE(4)
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

void update_curr_data_in_start(struct stage_data *curr_data, struct update_params *params) {
    if (curr_data && params) {
        curr_data->start_count += 1;
        curr_data->major = params->major;
        curr_data->first_minor = params->first_minor;
        blk_fill_rwbs(curr_data->io_type, params->cmd_flags);
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

// start rq_driver
SEC("kprobe/blk_mq_start_request") 
int kprobe_blk_mq_start_request(struct pt_regs *regs) 
{
    struct request *rq = (struct request *)PT_REGS_PARM1(regs);
    struct gendisk *curr_rq_disk = _(rq->rq_disk);

    u32 key_version = 1;
    struct version_map_num *version_map = bpf_map_lookup_elem(&version_res, &key_version);
    if (version_map) {
        if (version_map->num == VERSION_KY_V2401) {
            struct request_kylin_v2401 *rq = (struct request_kylin_v2401 *)PT_REGS_PARM1(regs);
            curr_rq_disk = _(rq->rq_disk);
        } else if (version_map->num == VERSION_KY_V2101) {
            struct request_kylin_v2101 *rq = (struct request_kylin_v2101 *)PT_REGS_PARM1(regs);
            curr_rq_disk = _(rq->rq_disk);
        }
        
    }
    
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
    if (counterp || major == 0) {
        return 0;
    }
    long err = bpf_map_update_elem(&blk_map, &rq, &zero, BPF_NOEXIST); 
    if (err) {
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
        if (key < MAP_SIZE) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }

    return 0; 
}

// finish rq_driver
SEC("kprobe/blk_mq_free_request") 
int kprobe_blk_mq_free_request(struct pt_regs *regs) 
{
    struct request *rq = (struct request *)PT_REGS_PARM1(regs);
    struct gendisk *curr_rq_disk = _(rq->rq_disk);

    u32 key_version = 1;
    struct version_map_num *version_map = bpf_map_lookup_elem(&version_res, &key_version);
    if (version_map) {
        if (version_map->num == VERSION_KY_V2401) {
            struct request_kylin_v2401 *rq = (struct request_kylin_v2401 *)PT_REGS_PARM1(regs);
            curr_rq_disk = _(rq->rq_disk);
        } else if (version_map->num == VERSION_KY_V2101) {
            struct request_kylin_v2101 *rq = (struct request_kylin_v2101 *)PT_REGS_PARM1(regs);
            curr_rq_disk = _(rq->rq_disk);
        }
    }

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
        update_curr_data_in_finish(curr_data, &params, &duration);
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
        if (key < MAP_SIZE) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }

    return 0; 
}

// finish bio
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
        update_curr_data_in_finish(curr_data, &params, &duration);
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

// start wbt
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
        if (key < MAP_SIZE) {
            __sync_fetch_and_add(&curr_data_time_range->count[key], 1);
        }
    }

    return 0; 
}

// finish wbt
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
        update_curr_data_in_finish(curr_data, &params, &duration);
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

// start get_tag 
SEC("kprobe/blk_mq_get_tag") 
int kprobe_blk_mq_get_tag(struct pt_regs *regs) 
{
    u64 tagkey = bpf_get_current_task();
    u64 value = (u64)PT_REGS_PARM1(regs);
    (void)bpf_map_update_elem(&tag_args, &tagkey, &value, BPF_ANY);
    struct blk_mq_alloc_data *bd= (struct blk_mq_alloc_data *)value;
    struct request_queue *q = (struct request_queue *)_(bd->q);
    struct backing_dev_info *backing_dev_info = (struct backing_dev_info *)_(q->backing_dev_info);
    struct device *owner = _(backing_dev_info->owner);

    u32 key_version = 1;
    struct version_map_num *version_map = bpf_map_lookup_elem(&version_res, &key_version);
    if (version_map) {
        if (version_map->num == VERSION_KY_V2401) {
            struct request_queue_kylin_v2401 *q = (struct request_queue_kylin_v2401 *)_(bd->q);
            struct backing_dev_info_kylin_v2401 *backing_dev_info = (struct backing_dev_info_kylin_v2401 *)_(q->backing_dev_info);
            owner = _(backing_dev_info->owner);
        }
    }
    
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
    if (counterp || major == 0) {
        return 0;
    }
        
    long err = bpf_map_update_elem(&tag_map, &tagkey, &zero, BPF_NOEXIST); 
    if (err) {
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
        if (key < MAP_SIZE) {
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
    u64 *tagargs = NULL;
    struct blk_mq_alloc_data *bd = NULL;

    tagargs = (u64 *)bpf_map_lookup_elem(&tag_args, &tagkey);
    if (tagargs == NULL) {
        bpf_map_delete_elem(&tag_args, &tagkey);
        return 0;
    }
    bd = (struct blk_mq_alloc_data *)(*tagargs);
    struct request_queue *q = _(bd->q);
    struct backing_dev_info *backing_dev_info = _(q->backing_dev_info);
    struct device *owner = _(backing_dev_info->owner);

    u32 key_version = 1;
    struct version_map_num *version_map = bpf_map_lookup_elem(&version_res, &key_version);
    if (version_map) {
        if (version_map->num == VERSION_KY_V2401) {
            struct request_queue_kylin_v2401 *q = (struct request_queue_kylin_v2401 *)_(bd->q);
            struct backing_dev_info_kylin_v2401 *backing_dev_info = (struct backing_dev_info_kylin_v2401 *)_(q->backing_dev_info);
            owner = _(backing_dev_info->owner);
        }
    }

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
        update_curr_data_in_finish(curr_data, &params, &duration);
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

char LICENSE[] SEC("license") = "Dual BSD/GPL"; 
u32 _version SEC("version") = LINUX_VERSION_CODE; 
