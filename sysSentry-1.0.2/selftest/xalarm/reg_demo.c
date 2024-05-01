/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: register alarm
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xalarm/register_xalarm.h>


#define SLEEP_TIME 10
#define ALARM_ID_1 1001
#define ALARM_ID_2 1002
#define ALARM_ID_3 1003
#define ID_LIST_LENGTH 3
#define ID_INDEX_0 0
#define ID_INDEX_1 1
#define ID_INDEX_2 2
#define REGISTER_TIMES 1
#define ARG_NUM 2
#define REGISTER_TIMES_INDEX 1

void RefreshOutput(void)
{
    int ret = fflush(stdout);
    if (ret != 0) {
        printf("failed to fflush\n");
    }
}
 
void callback(struct alarm_info *param)
{
    int alarmid, alarmlevel, alarmtype;
    long long alarmtime;
    char *pucParas;

    alarmid = xalarm_getid(param);
    alarmlevel = xalarm_getlevel(param);
    alarmtype = xalarm_gettype(param);
    alarmtime = xalarm_gettime(param);
    pucParas = xalarm_getdesc(param);
 
    printf("[alarmid:%d] [alarmlevel:%d] [alarmtype:%d] [alarmtime:%lld ms] [msg:%s]\n",
        alarmid, alarmlevel, alarmtype, alarmtime, pucParas);

    RefreshOutput();
}

int main(int argc, char **argv)
{
    int clientId;
    int registerTimes = REGISTER_TIMES;
    int ret;
    struct alarm_subscription_info id_filter;
 
    printf("demo start\n");
 
    id_filter.id_list[ID_INDEX_0] = ALARM_ID_1;
    id_filter.id_list[ID_INDEX_1] = ALARM_ID_2;
    id_filter.id_list[ID_INDEX_2] = ALARM_ID_3;
    id_filter.len = ID_LIST_LENGTH;
 
    if (argc == ARG_NUM) {
        registerTimes = atoi(argv[REGISTER_TIMES_INDEX]);
    }
    for (int i = 0;i < registerTimes; ++i) {
        clientId = xalarm_Register(callback, id_filter);
    }
    if (clientId < 0) {
        printf("demo register failed\n");
        return -1;
    } else {
        printf("xalarm register success, client id is %d\n", clientId);
    }
    printf("demo waiting alarm\n");
    RefreshOutput();
    sleep(SLEEP_TIME);

    xalarm_UnRegister(clientId);
    printf("unregister xalarm success\n");
    RefreshOutput();
    return 0;
}

