/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: send alarm
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xalarm/register_xalarm.h>


#define ALARMID 1002
#define LEVEL 2
#define TYPE 1
#define ARG_NUM 5
#define ALARM_ID_ARG_INDEX 1
#define ALARM_LEVEL_ARG_INDEX 2
#define ALARM_TYPE_ARG_INDEX 3
#define ALARM_MSG_ARG_INDEX 4

int main(int argc, char **argv)
{
    int alarmId = ALARMID;
    int level = LEVEL;
    int type = TYPE;
    unsigned char *msg = "test messages\0";
    
    if (argc == ARG_NUM) {
        alarmId = atoi(argv[ALARM_ID_ARG_INDEX]);
        level = atoi(argv[ALARM_LEVEL_ARG_INDEX]);
        type = atoi(argv[ALARM_TYPE_ARG_INDEX]);
        msg = argv[ALARM_MSG_ARG_INDEX];
    }

    xalarm_Report(alarmId, level, type, msg);

    return 0;
}
