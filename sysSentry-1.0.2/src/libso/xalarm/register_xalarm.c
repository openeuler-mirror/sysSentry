/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: inspection message alarm program
 * Author: Lan Sheng
 * Create: 2023-10-23
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <securec.h>
#include <signal.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include "register_xalarm.h"

#define DIR_XALARM "/var/run/xalarm"
#define PATH_REG_ALARM "/var/run/xalarm/alarm"
#define PATH_REPORT_ALARM "/var/run/xalarm/report"
#define ALARM_DIR_PERMISSION 0750
#define ALARM_SOCKET_PERMISSION 0700
#define TIME_UNIT_MILLISECONDS 1000

#define MAX_PARAS_LEN 511
#define MIN_ALARM_ID 1001
#define MAX_ALARM_ID (MIN_ALARM_ID + MAX_NUM_OF_ALARM_ID - 1)

#define ALARM_ENABLED 1
#define RECV_DELAY_MSEC 100

struct alarm_register_info {
    char alarm_enable_bitmap[MAX_NUM_OF_ALARM_ID];
    int register_fd;
    pthread_t register_tid;
    bool is_registered;
    alarm_callback_func callback;
    int thread_should_stop;
};

struct alarm_register_info g_register_info = {{0}, -1, ULONG_MAX, false, NULL, 1};

static bool id_is_registered(unsigned short alarm_id)
{
    if (alarm_id < MIN_ALARM_ID || alarm_id > MAX_ALARM_ID) {
        return false;
    }

    return g_register_info.alarm_enable_bitmap[alarm_id - MIN_ALARM_ID] == ALARM_ENABLED;
}

static void put_alarm_info(struct alarm_info *info)
{
    if ((g_register_info.callback != NULL) && (info != NULL) && (id_is_registered(info->usAlarmId))) {
        g_register_info.callback(info);
    }
}

static int create_unix_socket(const char *path)
{
    struct sockaddr_un alarm_addr;
    int fd = -1;
    int ret = 0;
    int flags;

    if (path == NULL || path[0] == '\0') {
        printf("create_unix_socket path is null");
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        printf("socket failed:%s\n", strerror(errno));
        return -1;
    }
    flags = fcntl(fd, F_GETFL, 0);
    ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
        printf("%s: fcntl setfl failed\n", __func__);
        goto release_socket;
    }

    if (access(PATH_REG_ALARM, F_OK) == 0) {
        ret = unlink(PATH_REG_ALARM);
        if (ret != 0) {
            printf("unlink register socket file failed\n");
            goto release_socket;
        }
    }

    if (access(DIR_XALARM, F_OK) == -1) {
        if (mkdir(DIR_XALARM, ALARM_DIR_PERMISSION) == -1) {
            printf("mkdir %s failed\n", DIR_XALARM);
            goto release_socket;
        }
    }

    ret = memset_s(&alarm_addr, sizeof(alarm_addr), 0, sizeof(alarm_addr));
    if (ret != 0) {
        printf("create_unix_socket:  memset_s alarm_addr failed, ret: %d\n", ret);
        goto remove_dir;
    }
    alarm_addr.sun_family = AF_UNIX;
    ret = strncpy_s(alarm_addr.sun_path, sizeof(alarm_addr.sun_path), path, sizeof(alarm_addr.sun_path) - 1);
    if (ret != 0) {
        printf("create_unix_socket: strncpy_s alarm_addr.sun_path failed, ret: %d\n", ret);
        goto remove_dir;
    }
    if (bind(fd, (struct sockaddr *)&alarm_addr, sizeof(alarm_addr.sun_family) + strlen(alarm_addr.sun_path)) < 0) {
        printf("bind socket failed:%s\n", strerror(errno));
        goto remove_dir;
    }
    if (chmod(path, ALARM_SOCKET_PERMISSION) < 0) {
        printf("chmod %s failed: %s\n", path, strerror(errno));
        goto unlink_sockfile;
    }

    return fd;

unlink_sockfile:
    ret = unlink(PATH_REG_ALARM);
    if (ret != 0) {
        printf("unlink register socket file failed\n");
    }
remove_dir:
    ret = rmdir(DIR_XALARM);
    if (ret != 0) {
        printf("rmdir %s failed: %s\n", path, strerror(errno));
    }
release_socket:
    (void)close(fd);

    return -1;
}

static void *alarm_recv(void *arg)
{
    int recvlen = 0;
    struct alarm_info info;
    int ret = 0;
    
    /* prctl does not return false if arg2 is right when arg1 is PR_SET_NAME */
    ret = prctl(PR_SET_NAME, "register-recv");
    if (ret != 0) {
        printf("alarm_recv: prctl set thread name failed\n");
        return NULL;
    }
    while (!g_register_info.thread_should_stop) {
        recvlen = recv(g_register_info.register_fd, &info, sizeof(struct alarm_info), 0);
        if (recvlen == (int)sizeof(struct alarm_info)) {
            put_alarm_info(&info);
        } else if (recvlen < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(RECV_DELAY_MSEC * TIME_UNIT_MILLISECONDS);
                continue;
            }
            printf("recv error len:%d errno:%d\n", recvlen, errno);
        }
    }
    return NULL;
}

static pthread_t create_thread(void)
{
    int ret;
    pthread_t t_id = ULONG_MAX;

    ret = pthread_create(&t_id, NULL, alarm_recv, NULL);
    if (ret < 0) {
        printf("create_thread: pthread_create error ret:%d\n", ret);
        t_id = ULONG_MAX;
    }
    return t_id;
}

static void set_alarm_id(struct alarm_subscription_info id_filter)
{
    int i;

    memset_s(g_register_info.alarm_enable_bitmap, MAX_NUM_OF_ALARM_ID * sizeof(char),
        0, MAX_NUM_OF_ALARM_ID * sizeof(char));
    for (i = 0; i < id_filter.len; i++) {
        g_register_info.alarm_enable_bitmap[id_filter.id_list[i] - MIN_ALARM_ID] = ALARM_ENABLED;
    }
}

static bool alarm_subscription_verify(struct alarm_subscription_info id_filter)
{
    int i;

    if (id_filter.len > MAX_NUM_OF_ALARM_ID) {
        return false;
    }

    for (i = 0; i < id_filter.len; i++) {
        if (id_filter.id_list[i] < MIN_ALARM_ID || id_filter.id_list[i] > MAX_ALARM_ID) {
            return false;
        }
    }
    return true;
}

bool xalarm_Upgrade(struct alarm_subscription_info id_filter, int client_id)
{
    if (!g_register_info.is_registered) {
        printf("%s: alarm has not registered, cannot upgrade\n", __func__);
        return false;
    }

    if (!alarm_subscription_verify(id_filter) || client_id != 0) {
        printf("%s: invalid args\n", __func__);
        return false;
    }
    set_alarm_id(id_filter);

    return true;
}

int xalarm_Register(alarm_callback_func callback, struct alarm_subscription_info id_filter)
{
    if (g_register_info.is_registered || (g_register_info.register_fd != -1) ||
        (g_register_info.register_tid != ULONG_MAX)) {
        printf("%s: alarm has registered\n", __func__);
        return -1;
    }

    if (!alarm_subscription_verify(id_filter) || callback == NULL) {
        printf("%s: param is invalid\n", __func__);
        return -1;
    }

    g_register_info.register_fd = create_unix_socket(PATH_REG_ALARM);
    if (g_register_info.register_fd == -1) {
        printf("%s: create_unix_socket failed\n", __func__);
        return -1;
    }

    g_register_info.thread_should_stop = 0;
    g_register_info.register_tid = create_thread();
    if (g_register_info.register_tid == ULONG_MAX) {
        printf("%s: create_thread failed\n", __func__);
        (void)close(g_register_info.register_fd);
        g_register_info.register_fd = -1;
        g_register_info.thread_should_stop = 1;
        return -1;
    }

    set_alarm_id(id_filter);
    g_register_info.callback = callback;
    g_register_info.is_registered = true;
    return 0;
}

void xalarm_UnRegister(int client_id)
{
    int ret;

    if (!g_register_info.is_registered) {
        printf("%s: alarm has not registered\n", __func__);
        return;
    }

    if (client_id != 0) {
        printf("%s: invalid client\n", __func__);
        return;
    }

    if (g_register_info.register_tid != ULONG_MAX) {
        g_register_info.thread_should_stop = 1;
        pthread_join(g_register_info.register_tid, NULL);
        g_register_info.register_tid = ULONG_MAX;
    }

    if (g_register_info.register_fd != -1) {
        (void)close(g_register_info.register_fd);
        g_register_info.register_fd = -1;
        ret = unlink(PATH_REG_ALARM);
        if (ret != 0) {
            printf("%s: unlink register socket file failed\n", __func__);
        }
    }

    memset_s(g_register_info.alarm_enable_bitmap, MAX_NUM_OF_ALARM_ID * sizeof(char),
        0, MAX_NUM_OF_ALARM_ID * sizeof(char));
    g_register_info.callback = NULL;
    g_register_info.is_registered = false;
}

/* return 0 if invalid id\level\type */
unsigned short xalarm_getid(const struct alarm_info *palarm)
{
    return palarm == NULL ? 0 : palarm->usAlarmId;
}

unsigned char xalarm_getlevel(const struct alarm_info *palarm)
{
    return palarm == NULL ? 0 : palarm->ucAlarmLevel;
}

unsigned char xalarm_gettype(const struct alarm_info *palarm)
{
    return palarm == NULL ? 0 : palarm->ucAlarmType;
}

long long xalarm_gettime(const struct alarm_info *palarm)
{
    return palarm == NULL ? 0 : ((long long)(palarm->AlarmTime.tv_sec) * TIME_UNIT_MILLISECONDS +
            (long long)(palarm->AlarmTime.tv_usec) / TIME_UNIT_MILLISECONDS);
}

char *xalarm_getdesc(const struct alarm_info *palarm)
{
    return palarm == NULL ? NULL : (char *)palarm->pucParas;
}

static int init_report_addr(struct sockaddr_un *alarm_addr)
{
    int ret;

    if (alarm_addr == NULL) {
        fprintf(stderr, "%s: alarm_addr is null\n", __func__);
        return -1;
    }

    ret = memset_s(alarm_addr, sizeof(struct sockaddr_un), 0, sizeof(struct sockaddr_un));
    if (ret != 0) {
        fprintf(stderr, "%s: memset_s  alarm_addr failed, ret: %d\n", __func__, ret);
        return -1;
    }
    alarm_addr->sun_family = AF_UNIX;
    ret = strncpy_s(alarm_addr->sun_path, sizeof(alarm_addr->sun_path), PATH_REPORT_ALARM,
        sizeof(alarm_addr->sun_path) - 1);
    if (ret != 0) {
        fprintf(stderr, "%s: strncpy_s alarm_addr->sun_path failed, ret: %d\n", __func__, ret);
        return -1;
    }

    return 0;
}

int xalarm_Report(unsigned short usAlarmId, unsigned char ucAlarmLevel,
    unsigned char ucAlarmType, char *pucParas)
{
    int ret, fd;
    struct alarm_info info;
    struct sockaddr_un alarm_addr;

    if ((usAlarmId < MIN_ALARM_ID || usAlarmId > MAX_ALARM_ID) ||
        (ucAlarmLevel < ALARM_LEVEL_FATAL || ucAlarmLevel > ALARM_LEVEL_DEBUG) ||
        (ucAlarmType < ALARM_TYPE_OCCUR || ucAlarmType > ALARM_TYPE_RECOVER)) {
        fprintf(stderr, "%s: alarm info invalid\n", __func__);
        return -1;
    }

    ret = memset_s(&info, sizeof(struct alarm_info), 0, sizeof(struct alarm_info));
    if (ret != 0) {
        fprintf(stderr, "%s: memset_s info failed, ret: %d\n", __func__, ret);
        return -1;
    }
    info.usAlarmId = usAlarmId;
    info.ucAlarmLevel = ucAlarmLevel;
    info.ucAlarmType = ucAlarmType;
    gettimeofday(&info.AlarmTime, NULL);
    if (pucParas != NULL) {
        ret = strncpy_s((char *)info.pucParas, MAX_PARAS_LEN, (char *)pucParas, MAX_PARAS_LEN - 1);
        if (ret != 0) {
            fprintf(stderr, "%s: strncpy_s  info.pucParas failed, ret: %d\n", __func__, ret);
            return -1;
        }
    }

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        fprintf(stderr, "%s socket create error: %s\n", __func__, strerror(errno));
        return -1;
    }

    ret = init_report_addr(&alarm_addr);
    if (ret == -1) {
        close(fd);
        return -1;
    }

    while (true) {
        ret = sendto(fd, &info, sizeof(struct alarm_info), 0, (struct sockaddr *)&alarm_addr,
            sizeof(alarm_addr.sun_family) + strlen(alarm_addr.sun_path));
        if (ret < 0) {
            if (errno == EINTR) {
                /* interrupted by signal, ignore */
                continue;
            } else {
                fprintf(stderr, "%s: sendto failed errno: %d\n", __func__, errno);
            }
        } else if (ret == 0) {
            fprintf(stderr, "%s: sendto failed, ret is 0\n", __func__);
        } else {
            if (ret != (int)sizeof(struct alarm_info)) {
                fprintf(stderr, "%s sendto failed, ret:%d, len:%u\n", __func__, ret, sizeof(struct alarm_info));
            }
        }
        break;
    }
    close(fd);

    return (ret > 0) ? 0 : -1;
}
