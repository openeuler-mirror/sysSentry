/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: unreg alarm
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xalarm/register_xalarm.h>

#define DEFAULT_CLIENT_ID 0
#define ARG_NUM 2
#define CLIENT_ID_ARG_INDEX 1

int main(int argc, char **argv)
{
    unsigned char msg[ALARM_INFO_MAX_PARAS_LEN] = "test messages";

    int clientId = DEFAULT_CLIENT_ID;

    if (argc == ARG_NUM) {
        clientId = atoi(argv[CLIENT_ID_ARG_INDEX]);
    }

    printf("unregister xalarm, client id is %d.", clientId);
    xalarm_UnRegister(clientId);
    int ret = fflush(stdout);
    if (ret != 0) {
        return -1;
    }
    return 0;
}
