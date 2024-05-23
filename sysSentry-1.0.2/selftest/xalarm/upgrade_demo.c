/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: upgrade alarm
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xalarm/register_xalarm.h>

#define SLEEP_TIME 10
#define ID_FILTER_LEN_ARG_INDEX 1
#define CLIENT_ID_ARG_INDEX 2
#define MIN_ARG_NUMS 3
#define ID_FILTER_BEGIN_INDEX 3
#define ID_INDEX_0 0
#define ALARM_ID_1 1001
#define ID_LIST_LENGTH 1

void RefreshOutput(void)
{
    int ret = fflush(stdout);
    if (ret != 0) {
        printf("failed to fflush\n");
    }
}

void Hook(struct alarm_info *param)
{
    int alarmId, alarmLevel, alarmType;
    long long alarmTime;
    char *pucParas;

    alarmId = xalarm_getid(param);
    alarmLevel = xalarm_getlevel(param);
    alarmType = xalarm_gettype(param);
    alarmTime = xalarm_gettime(param);
    pucParas = xalarm_getdesc(param);

    printf("alarmid:%d alarmlevel:%d alarmtype:%d alarmtime:%lld ms msg:%s\n",
        alarmId, alarmLevel, alarmType, alarmTime, pucParas);

    RefreshOutput();
}

int main(int argc, char **argv)
{
    struct alarm_subscription_info id_filter;
    int clientId;

    id_filter.id_list[ID_INDEX_0] = ALARM_ID_1;
    id_filter.len = ID_LIST_LENGTH;
    clientId = xalarm_Register(Hook, id_filter);
    if (clientId < 0) {
        printf("demo register failed\n");
        return -1;
    }
	
    // 参数：[filename, id_filter_len, client_id, filter_1, filter_2, ...]
    if (argc <= MIN_ARG_NUMS) {
        id_filter.len = 0;
        clientId = 0;
    } else {
        id_filter.len = atoi(argv[ID_FILTER_LEN_ARG_INDEX]);
        clientId = atoi(argv[CLIENT_ID_ARG_INDEX]);
    }

    for (int i = ID_FILTER_BEGIN_INDEX; i < argc; ++i) {
        id_filter.id_list[i - ID_FILTER_BEGIN_INDEX] = atoi(argv[i]);
    }

    printf("upgrade client id as %d", clientId);
    xalarm_Upgrade(id_filter, clientId);

    RefreshOutput();
    sleep(SLEEP_TIME);
    xalarm_UnRegister(clientId);
    return 0;
}
