#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "ras-events.h"
#include "non-standard-hbm-repair.h"

#define DEFAULT_LOG_LEVEL LOG_INFO
#define DEFAULT_PAGE_ISOLATION_THRESHOLD 128

int global_level_setting;
int page_isolation_threshold;

int string2int(const char* str, int* value)
{
    if (!str) {
        return -1;
    }
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if (errno != 0 || *endptr != '\0') {
        return -1;
    }
    *value = (int)val;
    if (val != (long)*value) {
        return -1;
    }
    return 0;
}

int execute_command(const char *command)
{
    FILE *fp;
    char buffer[128] = {0};
    int ret;
    fp = popen(command, "r");
    if (!fp) {
        log(LOG_ERROR, "popen failed\n");
        return -1;
    }

    fgets(buffer, sizeof(buffer), fp);
    log(LOG_DEBUG, "output of command is: %s\n", buffer);

    ret = pclose(fp);
    if (ret < 0) {
        log(LOG_ERROR, "pclose failed\n");
        return -1;
    }

    if (!WIFEXITED(ret)) {
        log(LOG_ERROR, "command did not terminate normally\n");
        return -1;
    }

    ret = WEXITSTATUS(ret);
    log(LOG_DEBUG, "command exited with status: %d\n", ret);
    return ret;
}

int load_required_driver(void)
{
    int ret;
    ret = execute_command("modprobe hisi_mem_ras 2>&1");
    if (ret < 0) {
        log(LOG_ERROR, "load repair driver failed\n");
        return ret;
    }
    ret = execute_command("modprobe page_eject 2>&1");
    if (ret < 0) {
        log(LOG_ERROR, "load page driver failed\n");
        return ret;
    }
    log(LOG_INFO, "load required driver success\n");
    return ret;
}

void hbm_param_init(void)
{
    int ret;
    char *env;

    env = getenv("HBM_ONLINE_REPAIR_LOG_LEVEL");
    ret = string2int(env, &global_level_setting);
    if (ret < 0) {
        global_level_setting = DEFAULT_LOG_LEVEL;
        log(LOG_WARNING, "Get log level from config failed, set the default value %d\n", DEFAULT_LOG_LEVEL);
    } else {
        log(LOG_INFO, "log level: %d\n", global_level_setting);
    }

    env = getenv("PAGE_ISOLATION_THRESHOLD");
    ret = string2int(env, &page_isolation_threshold);
    if (ret < 0) {
        page_isolation_threshold = DEFAULT_PAGE_ISOLATION_THRESHOLD;
        log(LOG_WARNING, "Get page_isolation_threshold from config failed, set the default value %d\n", DEFAULT_PAGE_ISOLATION_THRESHOLD);
    } else {
        log(LOG_INFO, "page_isolation_threshold: %d\n", page_isolation_threshold);
    }
}


int main(int argc, char *argv[])
{
    int ret;

    hbm_param_init();

    ret = load_required_driver();
    if (ret < 0) {
        log(LOG_DEBUG, "load required driver failed\n");
        return ret;
    }

    struct ras_events *ras = init_trace_instance();
    if (!ras)
        return -1;

    ret = toggle_ras_event(ras->tracing, "ras", "non_standard_event", 1);
    if (ret < 0) {
        log(LOG_WARNING, "unable to enable ras non_standard_event.\n");
        free(ras);
        return -1;
    }

    get_flash_total_size();

    handle_ras_events(ras);

    ret = toggle_ras_event(ras->tracing, "ras", "non_standard_event", 0);
    if (ret < 0) {
        log(LOG_WARNING, "unable to disable ras non_standard_event.\n");
    }

    free(ras);
    return ret;
}