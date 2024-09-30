/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * Description: ebpf collector program
 * Author: Zhang Nan
 * Create: 2024-09-27
 */
#include <argp.h> 
#include <signal.h>
#include <stdio.h> 
#include <unistd.h> 
#include <time.h> 
#include <stdlib.h> 
#include <string.h> 
#include <linux/sysinfo.h> 
#include <sys/resource.h> 
#include <bpf/bpf.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/sysmacros.h>
#include <bpf_load.h>
#include <dirent.h>
#include "ebpf_collector.h"

#define BLK_MAP    (map_fd[0]) 
#define BLK_RES    (map_fd[1]) 
#define BIO_MAP    (map_fd[2]) 
#define BIO_RES    (map_fd[3])
#define WBT_MAP    (map_fd[4])
#define WBT_RES    (map_fd[5]) 
#define TAG_MAP    (map_fd[7])
#define TAG_RES    (map_fd[8])
#define BPF_FILE   "/usr/lib64/ebpf_collector.bpf.o"

typedef struct {
    int major;
    int minor;
} DeviceInfo;

static volatile bool exiting; 

const char argp_program_doc[] = 
"Show block device I/O pattern.\n"
"\n"
"USAGE: ebpf_collector [--help]\n"
"\n"
"EXAMPLES:\n"
"    ebpf_collector              # show block I/O pattern\n";

static const struct argp_option opts[] = {
    { NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help" }, 
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state) {
    static int pos_args; 

    switch (key) { 
    case 'h': 
        argp_state_help(state, stderr, ARGP_HELP_STD_HELP); 
        break;
    default: 
        return ARGP_ERR_UNKNOWN; 
    }
    return 0; 
}

static void sig_handler(int sig)
{
    exiting = true; 
}

char* extract_device_name(const char *path) {
    const char *dev_dir = "/dev/";
    char *name = strrchr(path, '/') + 1; 
    if (strncmp(dev_dir, path, strlen(dev_dir)) == 0) {
        return strdup(name); 
    }
    return NULL; 
}

char* find_device_name(dev_t dev) {
    DIR *dir;
    struct dirent *entry;
    struct stat sb;
    char *device_name = NULL;
    char path[1024];

    dir = opendir("/dev");
    if (dir == NULL) {
        perror("Failed to open /dev");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);

        if (entry->d_type == DT_DIR || entry->d_type == DT_LNK) {
            continue;
        }

        if (stat(path, &sb) == -1) {
            continue; 
        }

        if (major(sb.st_rdev) == major(dev) && minor(sb.st_rdev) == minor(dev)) {
            device_name = extract_device_name(path); 
            break;
        }
    }

    closedir(dir);
    return device_name;
}

static int print_map_res(struct bpf_map *map_res, char *stage, int *map_size)
{
    struct stage_data counter; 
    int key = 0; 

    struct sysinfo info; 
    sysinfo(&info); 

    for (key = 0; key < map_size; key++) {
        int err;
        err = bpf_map_lookup_elem(map_res, &key, &counter); 
        if (err < 0) { 
            fprintf(stderr, "failed to lookup %s map_res: %d\n", stage, err); 
            return -1; 
        }
        
        size_t length = strlen(counter.io_type);
        char io_type;
        if (length > 0) {
            io_type = counter.io_type[0];
        } else {
            io_type = NULL;
        }
        int major = counter.major;
        int first_minor = counter.first_minor;
        dev_t dev = makedev(major, first_minor);    
        char *device_name = find_device_name(dev);
        if (device_name && io_type) {
            printf("%-7s %10llu %10llu %u %c %s\n",
                stage, 
                counter.finish_count, 
                counter.duration,
                counter.bucket[MAX_BUCKETS].io_count,
                io_type,
                device_name
            );
            fflush(stdout);
        }
    }

    return 0; 
}

int init_map(int *map_fd, const char *map_name, int *map_size, DeviceInfo *devices) {
    struct stage_data init_data = {0};
    memset(init_data.io_type, 0, sizeof(init_data.io_type));
    memset(init_data.bucket, 0, sizeof(init_data.bucket));

    for (int i = 0; i < map_size; i++) {
        init_data.major = devices[i].major;
        init_data.first_minor = devices[i].minor;
        if (bpf_map_update_elem(map_fd, &i, &init_data, BPF_ANY) != 0) {
            printf("Failed to initialize map %s at index %d\n", map_name, i);
            return 1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    struct partitions *partitions = NULL; 
    const struct partition *partition; 
    static const struct argp argp = { 
        .options = opts, 
        .parser = parse_arg, 
        .doc = argp_program_doc, 
    };
    int err; 
    char filename[256];
    DIR *dir;
    struct dirent *entry;
    char path[1024];
    int major, minor;
    DeviceInfo devices[MAP_SIZE];
    int device_count = 0;
    struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY}; 
    setrlimit(RLIMIT_MEMLOCK, &r); 

    err = argp_parse(&argp, argc, argv, 0, NULL, NULL); 
    if (err) 
        return err; 

    snprintf(filename, sizeof(filename), BPF_FILE);

    if (load_bpf_file(filename)) { 
        return 1; 
    }

    signal(SIGINT, sig_handler);

    dir = opendir("/dev");
    if (dir == NULL) {
        printf("Failed to open /dev directory");
        return EXIT_FAILURE;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_BLK) { 
            snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
            struct stat statbuf;
            if (lstat(path, &statbuf) == 0) {
                if (S_ISBLK(statbuf.st_mode)) {
                    devices[device_count].major = major(statbuf.st_rdev);
                    devices[device_count].minor = minor(statbuf.st_rdev);
                    device_count++;
                    if (device_count >= MAP_SIZE) {
                        break;
                    }
                }
            }
        }
    }

    closedir(dir);

    if (init_map(BLK_RES, "blk_res_map", device_count, devices) != 0) {
        return 1;
    }
    if (init_map(BIO_RES, "blo_res_map", device_count, devices) != 0) {
        return 1;
    }
    if (init_map(WBT_RES, "wbt_res_map", device_count, devices) != 0) {
        return 1;
    }
    if (init_map(TAG_RES, "tag_res_map", device_count, devices) != 0) {
        return 1;
    }

    for (;;) { 
       
        sleep(1); 

        err = print_map_res(BLK_RES, "rq_driver", device_count); 
        if (err) 
            break; 

        err = print_map_res(BIO_RES, "bio", device_count); 
        if (err) 
            break; 
        
        err = print_map_res(TAG_RES, "gettag", device_count); 
        if (err)	
            break; 
        
        err = print_map_res(WBT_RES, "wbt", device_count); 
        if (err) 
            break; 

        if (exiting) 
            break; 
    }

    return -err; 
}
