/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: inspection message alarm program
 * Author: Lan Sheng
 * Create: 2023-10-23
 */

#ifndef __REGISTER_XALARM_H
#define __REGISTER_XALARM_H

#include <sys/time.h>
#include <stdbool.h>
 
#define ALARM_INFO_MAX_PARAS_LEN 512
#define MAX_STRERROR_SIZE 1024
#define MAX_ALARM_TYEPS 1024
#define MIN_ALARM_ID 1001
#define BYTE_TO_BITS 8

#define MEMORY_ALARM_ID 1001

#define ALARM_LEVEL_FATAL 1
#define ALARM_LEVEL_ERROR 2
#define ALARM_LEVEL_WARNING 3
#define ALARM_LEVEL_INFO 4
#define ALARM_LEVEL_DEBUG 5

#define ALARM_TYPE_OCCUR 1
#define ALARM_TYPE_RECOVER 2

#define MAX_NUM_OF_ALARM_ID 128

/*
 * usAlarmId：unsigned short，告警id，某一类故障一个id，id定义避免重复。
 * ucAlarmLevel: 告警级别，从FATAL到DEBUG
 * ucAlarmType：unsigned char，告警类别，表示告警产生或告警恢复（对用户呈现需要用户操作的故障恢复）
 * AlarmTime：struct timeval，告警生成时间戳
 * pucParas：unsigned char*，告警描述信息
 */
/* socket 通信格式 */
struct alarm_info {
    unsigned short usAlarmId;
    unsigned char ucAlarmLevel;
    unsigned char ucAlarmType;
    struct timeval AlarmTime;
    char pucParas[ALARM_INFO_MAX_PARAS_LEN];
};
 
/*
 * hook回调函数处理
 * Notes        : 下述函数不支持多线程,不是信号安全函数
 */
typedef void (*alarm_callback_func)(struct alarm_info *palarm);

struct alarm_subscription_info {
    int id_list[MAX_NUM_OF_ALARM_ID];
    unsigned int len;
};

int xalarm_Register(alarm_callback_func callback, struct alarm_subscription_info id_filter);
void xalarm_UnRegister(int client_id);
bool xalarm_Upgrade(struct alarm_subscription_info id_filter, int client_id);
 
unsigned short xalarm_getid(const struct alarm_info *palarm);
unsigned char xalarm_gettype(const struct alarm_info *palarm);
unsigned char xalarm_getlevel(const struct alarm_info *palarm);
long long xalarm_gettime(const struct alarm_info *palarm);
char *xalarm_getdesc(const struct alarm_info *palarm);

int xalarm_Report(unsigned short usAlarmId,
        unsigned char ucAlarmLevel,
        unsigned char ucAlarmType,
        char *pucParas);
 
#endif
