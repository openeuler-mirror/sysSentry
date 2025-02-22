#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "register_xalarm.h"
#include "log_utils.h"
#include "smh_common_type.h"

#define TOOL_NAME "sentry_msg_monitor"
#define SMH_DEV_PATH "/dev/sentry_msg_helper"
#define PID_FILE_PATH "/var/run/"TOOL_NAME".pid"
#define ID_LIST_LENGTH SMH_MESSAGE_MAX
#define MSG_STR_MAX_LEN 128
#define DEFAULT_LOG_LEVEL LOG_INFO
#define MAX_RETRY_NUM 3
#define RETRY_PERIOD 1
#define XALARM_MSG_ITEM_CNT 2 // msgid_res
struct receiver_cleanup_data {
    struct alarm_msg *al_msg;
    struct alarm_register* register_info;
};

static int handle_file_lock(int fd, bool lock)
{
    int ret;
    struct flock fl;
    fl.l_type   = lock ? F_WRLCK : F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    ret = fcntl(fd, F_SETLK, &fl);
    if (ret < 0) {
        logging_error("fcntl failed, error msg is %s\n", strerror(errno));
    } else {
        logging_debug("fcntl success, lock ret code is %d\n", ret);
    }
    return ret;
}

static int check_and_set_pid_file()
{
    int ret, fd;
    fd = open(PID_FILE_PATH, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        logging_error("open file %s failed!\n", PID_FILE_PATH);
        return -1;
    }

    ret = handle_file_lock(fd, true);
    if (ret < 0) {
        logging_error("%s is already running\n", TOOL_NAME);
        close(fd);
        return ret;
    }

    return fd;
}

static int release_pid_file(int fd)
{
    int ret;
    ret = handle_file_lock(fd, false);
    if (ret < 0) {
        logging_error("release pid file %s lock failed, error msg is %s\n", PID_FILE_PATH, strerror(errno));
        return ret;
    }

    close(fd);
    ret = remove(PID_FILE_PATH);
    if (ret < 0) {
        logging_error("remove %s failed, error msg is %s\n", PID_FILE_PATH, strerror(errno));
    }
    return ret;
}

static int smh_dev_get_fd(void)
{
	int smh_dev_fd;
    smh_dev_fd = open(SMH_DEV_PATH, O_RDWR);
    if (smh_dev_fd < 0) {
        logging_error("Failed to open smh_dev_fd for %s.\n", SMH_DEV_PATH);
    }

	return smh_dev_fd;
}

static int convert_smh_msg_to_str(struct sentry_msg_helper_msg* smh_msg, char* str)
{
    char msgid_str[32];
    snprintf(msgid_str, sizeof(msgid_str), "%lu", smh_msg->msgid);

    int res = snprintf(str, MSG_STR_MAX_LEN, "%s", msgid_str);
    if ((size_t)res >= MSG_STR_MAX_LEN) {
        logging_warn("msg str size exceeds the max value\n");
        return -1;
    }
    return 0;
}

static int convert_str_to_smh_msg(char* str, struct sentry_msg_helper_msg* smh_msg)
{
    if (!(sscanf(str, "%lu_%d", &(smh_msg->msgid), &(smh_msg->res)) == XALARM_MSG_ITEM_CNT)) {
        logging_warn("Invalid msg str format, str is %s\n", str);
        return -1;
    }
    return 0;
}

static unsigned short get_xalarm_us_alarm_id(enum sentry_msg_helper_msg_type msg_type)
{
    unsigned short alarm_id = 0;
    switch (msg_type)
    {
    case SMH_MESSAGE_POWER_OFF:
        alarm_id = ALARM_REBOOT_EVENT;
        break;
    default:
        logging_warn("Unknown msg type: %d\n", msg_type);
        break;
    }
    return alarm_id;
}

static void sender_cleanup(void* arg)
{
    logging_debug("In sender thread cleanup\n");
    int fd = *(int *)arg;
    if (fd > 0) {
        close(fd);
    }
    logging_info("Sender thread cleanup over\n");
}

static void* sender_thread(void* arg) {
    int ret;
    int fd = smh_dev_get_fd();
    if (fd < 0) {
        goto close_recv;
    }
    pthread_cleanup_push(sender_cleanup, &fd);

    pthread_t partner_t;
    char str[MSG_STR_MAX_LEN];

    while (1) {
        struct sentry_msg_helper_msg smh_msg;
        errno = 0;
        ret = read(fd, &smh_msg, sizeof(struct sentry_msg_helper_msg));
        if (ret != sizeof(struct sentry_msg_helper_msg)) {
            if (errno == ERESTART || errno == EFAULT) {
                logging_warn("Read dev failed, return code (%d): try to read the next one msg from kernel\n", errno);
                continue;
            } else if (errno == EAGAIN) {
                logging_warn("Read dev failed, return code (%d): kernel queue is full, try to read again\n", errno);
                continue;
            } else {
                logging_error("Read dev failed, return code %d\n", errno);
                goto sender_err;
            }
        }
        logging_debug("Read dev success!\n");

        ret = convert_smh_msg_to_str(&smh_msg, str);
        if (ret < 0) {
            continue;
        }
        unsigned short al_type = get_xalarm_us_alarm_id(smh_msg.type);
        if (al_type == 0) {
            logging_warn("Send msg to xalarmd failed: Get unknown type msg, skip it\n");
            continue;
        }
        for (int i = 0; i < MAX_RETRY_NUM; i++) {
            ret = xalarm_report_event(al_type, str);
            if (ret == 0) {
                logging_info("Send msg success: al_type: %d, str: %s\n", al_type, str);
                break;
            }
            if (ret == -EINVAL) {
                logging_warn("Send msg to xalarmd failed: (%d) Invalid input value, skip it\n", ret);
                break;
            } else if (ret == -ENOTCONN || ret == -ECOMM) {
                logging_warn("Send msg to xalarmd failed: (%d) Bad socket conn, try again\n", ret);
                sleep(RETRY_PERIOD);
            } else if (ret < 0) {
                logging_warn("xalarm_report_event return %d\n", ret);
                break;
            }
        }
        if (ret == -ENOTCONN || ret == -ECOMM) {
            logging_warn("Send msg to xalarmd failed: (%d) Bad socket conn, skip it\n", ret);
        }
    }

sender_err:
    close(fd);
close_recv:
    partner_t = *(pthread_t*)arg;
    if (partner_t)
        pthread_cancel(partner_t);
    logging_error("Sender thread exited unexpectedly\n");
    pthread_cleanup_pop(0);
    return NULL;
}

static void receiver_cleanup(void* arg)
{
    logging_debug("In receiver thread cleanup\n");
    struct receiver_cleanup_data* rcd = (struct receiver_cleanup_data*) arg;
    if (rcd->al_msg) {
        free(rcd->al_msg);
    }
    if (rcd->register_info) {
        xalarm_unregister_event(rcd->register_info);
    }
    logging_info("Receiver thread cleanup over\n");
}

static void* receiver_thread(void* arg) {
    int ret, fd;
    struct alarm_msg *al_msg;
    struct sentry_msg_helper_msg smh_msg;
    pthread_t partner_t;
    struct alarm_register* register_info;

    fd = smh_dev_get_fd();
    if (fd < 0) {
        goto close_send;
    }

    al_msg = (struct alarm_msg*)malloc(sizeof(struct alarm_msg));
    if (!al_msg) {
        logging_error("malloc al_msg failed!\n");
        goto receiver_err;
    }

re_register:
    register_info = NULL;
    struct alarm_subscription_info id_filter = {
        .len = ID_LIST_LENGTH
    };
    id_filter.id_list[0] = ALARM_REBOOT_ACK_EVENT;

    for (int i = 0; i < MAX_RETRY_NUM; i++) {
        ret = xalarm_register_event(&register_info, id_filter);
        if (ret == 0)
            break;
        if (ret == -ENOTCONN) {
            logging_warn("Failed to register xalarm, try to re-register again\n");
            sleep(RETRY_PERIOD);
        } else {
            logging_error("xalarm_register_event return %d\n", ret);
            goto receiver_err;
        }
    }
    if (ret == -ENOTCONN) {
        logging_error("Failed to register xalarm: (%d) bad connection\n", ret);
        goto receiver_err;
    }

    struct receiver_cleanup_data rcd = {
        .al_msg = al_msg,
        .register_info = register_info
    };
    pthread_cleanup_push(receiver_cleanup, &rcd);

    while (1) {
        ret = xalarm_get_event(al_msg, register_info);
        if (ret == -ENOTCONN || ret == -ECONNRESET || ret == -EBADF) {
            logging_warn("Failed to get msg: (%d) Xalarmd service exception, try to re-register\n", ret);
            xalarm_unregister_event(register_info);
            goto re_register;
        } else if (ret < 0) {
            logging_error("xalarm_get_event return %d\n", ret);
            goto un_register;
        } else {
            logging_info("Get msg: al_type: %d, str: %s\n", al_msg->usAlarmId, al_msg->pucParas);
        }

        ret = convert_str_to_smh_msg(al_msg->pucParas, &smh_msg);
        if (ret < 0) {
            logging_warn("Convert str failed: Bad format '%s', skip it\n", al_msg->pucParas);
            continue;
        }
        for (int i = 0; i < MAX_RETRY_NUM; i++) {
            errno = 0;
            ret = ioctl(fd, SMH_MSG_ACK, &smh_msg);
            if (ret == 0)
                break;
            if (errno == ERESTART || errno == ETIME || errno == ENOENT) {
                logging_warn("Ack to kernel failed: ioctl return %d, skip it\n", errno);
                break;
            } else if (errno == EFAULT) {
                logging_warn("Ack to kernel failed: (%d) Copy from user failed, try again\n", errno);
                sleep(RETRY_PERIOD);
            } else if (ret < 0) {
                logging_error("Ack to kernel failed: ioctl return %d\n", errno);
                goto un_register;
            }
        }
        if (errno == EFAULT) {
            logging_warn("Ack to kernel failed: (%d) Copy from user failed, skip it\n", errno);
        }
    }

un_register:
    xalarm_unregister_event(register_info);
receiver_err:
    free(al_msg);
    close(fd);
close_send:
    partner_t = *(pthread_t*)arg;
    if (partner_t)
        pthread_cancel(partner_t);
    logging_error("Receiver thread exited unexpectedly\n");
    pthread_cleanup_pop(0);
    return NULL;
}

int main()
{
    int ret, pid_fd;
    pthread_t sender, receiver;

    pid_fd = check_and_set_pid_file();
    if (pid_fd < 0) {
        return pid_fd;
    }

    ret = pthread_create(&sender, NULL, sender_thread, &receiver);
    if (ret) {
        logging_error("Failed to create sender thread");
        goto err_release;
    }
    ret = pthread_create(&receiver, NULL, receiver_thread, &sender);

    if (ret) {
        logging_error("Failed to create receiver thread");
        pthread_cancel(sender);
        pthread_join(sender, NULL);
        goto err_release;
    }

    logging_info("sentry_msg_monitor start!\n");

    pthread_join(sender, NULL);
    pthread_join(receiver, NULL);

err_release:
    release_pid_file(pid_fd);
    return ret;
}
