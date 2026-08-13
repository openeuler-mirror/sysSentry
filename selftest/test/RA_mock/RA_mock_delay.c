#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <xalarm/register_xalarm.h>

#define SLEEP_TIME 10
#define ID_LIST_LENGTH 2
#define TIME_UNIT_MILLISECONDS 1000

void print_alarm_info(struct alarm_msg* param)
{
    int alarmid;
    long long alarmtime;
    char *pucParas;

    alarmid = (param == NULL ? 0 : param->usAlarmId);
    alarmtime = (param == NULL ? 0 : ((long long)param->AlarmTime.tv_sec) * TIME_UNIT_MILLISECONDS + (long long)(param->AlarmTime.tv_usec / TIME_UNIT_MILLISECONDS));
    pucParas = (param == NULL ? NULL : param->pucParas);
    if (!pucParas) {
        printf("pucParas is null");
    } else {
        printf("plugin notified with [alarmid:%d], [alarmtime: %lld ms], [msg:%s]\n", alarmid, alarmtime, pucParas);
    }
    return;
}

int main(int argc, char **argv)
{
    struct alarm_msg msg;
    struct alarm_register* register_info;
    struct alarm_subscription_info id_filter;

    id_filter.id_list[0] = ALARM_REBOOT_EVENT;
    id_filter.id_list[1] = ALARM_OOM_EVENT;
    id_filter.len = ID_LIST_LENGTH;

    int ret = xalarm_register_event(&register_info, id_filter);
    if (ret < 0) {
        perror("Failed to register xalarm\n");
        return 1;
    }

    while (true) {
        printf("Waiting for plugin msg ... \n");

        ret = xalarm_get_event(&msg, register_info);
        if (ret < 0) {
            printf("Failed to get msg, ret is [%d]\n", ret);
            break;
        }

        print_alarm_info(&msg);
    }

    xalarm_unregister_event(&register_info);
    return 0;
}
