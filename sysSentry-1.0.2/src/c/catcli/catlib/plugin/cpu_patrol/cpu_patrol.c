#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include "cpu_patrol_result.h"
#include "cpu_patrol.h"

pthread_mutex_t g_start_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_stop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_stop_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned short g_CONVERT_TO_PERCENTAGE = 100;
bool g_stop_flag = false;


static void set_stop_flag(bool flag)
{
    pthread_mutex_lock(&g_stop_flag_mutex);

    g_stop_flag = flag;

    if (pthread_mutex_unlock(&g_stop_flag_mutex) != 0) {
        CAT_LOG_E("pthread_mutex_unlock g_stop_flag_mutex failed.");
    }
}

static cat_return_t write_patrol_config(const char *path, const char *config)
{
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        CAT_LOG_E("Open path[%s] file fail, errno[%d]", path, errno);
        return CAT_GENERIC_ERROR;
    }

    ssize_t ret = write(fd, config, strlen(config));
    if (ret == -1) {
        CAT_LOG_E("Write [%s] file fail, config[%s], errno[%d]", path, config, errno);
        close(fd);
        if (errno == EINVAL) {
            return CAT_INVALID_PARAMETER;
        }
        return CAT_GENERIC_ERROR;
    }

    close(fd);
    return CAT_OK;
}

static cat_return_t set_patrol_cpumask(const char *cpumask)
{
    return write_patrol_config(KERNEL_INTF_PATH_CPUMASK, cpumask);
}

static cat_return_t set_patrol_times(unsigned int patrol_times)
{
    char tmpstr[DECIMAL_STR_LEN] = {0};

    int ret = snprintf(tmpstr, sizeof(tmpstr), "%u", patrol_times);
    if (ret <= 0) {
        CAT_LOG_E("Get patrol_times string fail, %d", ret);
        return CAT_GENERIC_ERROR;
    }

    return write_patrol_config(KERNEL_INTF_PATH_PATROL_TIMES, tmpstr);
}

static cat_return_t set_patrol_cpu_utility(unsigned int utility)
{
    char tmpstr[DECIMAL_STR_LEN] = {0};

    int ret = snprintf(tmpstr, sizeof(tmpstr), "%u", utility);
    if (ret <= 0) {
        CAT_LOG_E("Get utility string fail, %d", ret);
        return CAT_GENERIC_ERROR;
    }

    return write_patrol_config(KERNEL_INTF_PATH_CPU_UTILITY, tmpstr);
}

static cat_return_t check_cpumask(const char *cpumask)
{
    regex_t reg = { 0 };
    const char *pattern = "^([0-9]+(-[0-9]+)*,?)+$";

    // 编译正则模式串
    if (regcomp(&reg, pattern, REG_EXTENDED) != 0) {
        CAT_LOG_E("regcomp error");
        return CAT_GENERIC_ERROR;
    }

    const size_t nmatch = 1;      // 定义匹配结果最大允许数
    regmatch_t pmatch[1] = {0}; // 定义匹配结果在待匹配串中的下标范围
    int status = regexec(&reg, cpumask, nmatch, pmatch, 0);
    regfree(&reg); // 释放正则表达式
    if (status != 0) {
        CAT_LOG_E("CPU mask check failed： '%s' is invalid format", cpumask);
        return CAT_INVALID_PARAMETER;
    }

    return CAT_OK;
}

static cat_return_t start_patrol(bool is_start)
{
    int fd = open("/sys/devices/system/cpu/cpuinspect/start_patrol", O_WRONLY);
    if (fd < 0) {
        CAT_LOG_E("Open start_patrol file fail, %s", strerror(errno));
        return CAT_GENERIC_ERROR;
    }

    char start_switch = (char)((is_start != 0) ? '1' : '0');
    int ret = write(fd, &start_switch, sizeof(start_switch));
    if (ret < 0) {
        CAT_LOG_E("Write cpu_utility file fail, %s", strerror(errno));
        close(fd);
        return CAT_GENERIC_ERROR;
    }

    close(fd);
    return CAT_OK;
}

static bool is_patrol_running(void)
{
    int fd = open("/sys/devices/system/cpu/cpuinspect/patrol_complete", O_RDONLY);
    if (fd < 0) {
        CAT_LOG_E("Open patrol_complete file fail, %s", strerror(errno));
        return false;
    }

    // 查询上一次巡检是否已经结束，'1' 结束，'0' 未结束
    char buf = '0';
    if (read(fd, &buf, sizeof(buf)) > 0) {
        if (buf == '1') {
            close(fd);
            return false;
        }
    }
    close(fd);

    return true;
}

static cat_return_t fill_patrol_para(const char *cpumask, int cpu_utility, int patrol_second,
    cpu_patrol_para *patrol_para)
{
    if (check_cpumask(cpumask) != CAT_OK) {
        return CAT_INVALID_PARAMETER;
    }
    patrol_para->cpumask = cpumask;
    if (patrol_second <= 0) {
        CAT_LOG_E("patrol second must be greater than 0");
        return CAT_INVALID_PARAMETER;
    }
    patrol_para->patrol_second = patrol_second;
    if (cpu_utility > MAX_CPU_UTILITY || cpu_utility < 0) {
        CAT_LOG_E("the range of cpu utility is [0,100]");
        return CAT_INVALID_PARAMETER;
    }
    patrol_para->cpu_utility = cpu_utility;
    // 内核一次巡检的用例次数，根据cpu利用率动态调整，至少为1次
    patrol_para->patrol_times = PATROL_TIMES_PER_ITERATION * patrol_para->cpu_utility / g_CONVERT_TO_PERCENTAGE;
    patrol_para->patrol_times = (patrol_para->patrol_times == 0 ? 1 : patrol_para->patrol_times);

    return CAT_OK;
}

static cat_return_t set_patrol_policy(const cpu_patrol_para *patrol_para)
{
    cat_return_t ret = set_patrol_cpumask(patrol_para->cpumask);
    if (ret != CAT_OK) {
        return ret;
    }
    ret = set_patrol_times(patrol_para->patrol_times);
    if (ret != CAT_OK) {
        return ret;
    }

    (void)set_patrol_cpu_utility(patrol_para->cpu_utility);

    CAT_LOG_I("Set policy, cpumask[%s], times[%d], utility[%d%%]", patrol_para->cpumask, patrol_para->patrol_times,
              patrol_para->cpu_utility);

    return CAT_OK;
}

static cat_return_t run(const cpu_patrol_para *patrol_para)
{
    CAT_LOG_I("Cpu patrol begin,it will cost about %d(s)", patrol_para->patrol_second);
    // 内核巡检基本配置
    cat_return_t ret = set_patrol_policy(patrol_para);
    if (ret != CAT_OK) {
        return ret;
    }
    (void) signal(SIGTERM, lib_cpu_patrol_stop); // kill
    (void) signal(SIGINT, lib_cpu_patrol_stop);  // ctrl+c
    // 分多次进行巡检，防止巡检调度进程被杀掉后，内核态的巡检还长时间未结束。内核一次巡检的时长跟PATROL_TIMES_PER_ITERATION成正比
    const int wait_time = 500 * 1000; // 等待500毫秒

    struct timespec start_timespec;
    int start_time_ret = clock_gettime(CLOCK_MONOTONIC, &start_timespec);
    if (start_time_ret == -1) {
        CAT_LOG_E("get system clock failed");
        return CAT_ERR;
    }
    struct tm start_tm;
    struct tm *start_tm_ret = localtime_r(&start_timespec.tv_sec, &start_tm);
    if (start_tm_ret == NULL) {
        CAT_LOG_E("convert timestamp to localtime failed");
        return CAT_ERR;
    }
    time_t start_time = mktime(&start_tm);

    while (!g_stop_flag) {
        struct timespec end_timespec;
        int end_time_ret = clock_gettime(CLOCK_MONOTONIC, &end_timespec);
        if (end_time_ret == -1) {
            CAT_LOG_E("get system clock failed");
            return CAT_ERR;
        }
        struct tm end_tm;
        struct tm *end_tm_ret = localtime_r(&end_timespec.tv_sec, &end_tm);
        if (end_tm_ret == NULL) {
            CAT_LOG_E("convert timestamp to localtime failed");
            return CAT_ERR;
        }
        time_t end_time = mktime(&end_tm);
        double diff_time = difftime(end_time, start_time);
        if (diff_time >= patrol_para->patrol_second) {
            break;
        }
        ret = start_patrol(true);
        if (ret != CAT_OK) {
            break;
        }
        // 巡检是异步下发的，等待500毫秒，巡检开始执行后，再查询结果
        usleep(wait_time);
        // 等待巡检结束，每500毫秒查询一次
        while (is_patrol_running()) {
            usleep(wait_time);
        }
        handle_patrol_result();
    }
    return CAT_OK;
}

cat_return_t start(const char *cpumask, int cpu_utility, int patrol_second, cpu_set_t *cpu_set)
{
    if (is_patrol_running()) {
        CAT_LOG_E("AN cpu patrol instance is already running.");
        return CAT_ALREADY_RUNNING;
    }
    set_stop_flag(false);

    // 1、先清空上一次的巡检结果
    clear_patrol_result();

    // 2、解析巡检参数
    cpu_patrol_para patrol_para = { 0 };
    patrol_para.patrol_times = PATROL_TIMES_PER_ITERATION;
    cat_return_t ret = fill_patrol_para(cpumask, cpu_utility, patrol_second, &patrol_para);
    if (ret != CAT_OK) {
        return ret;
    }

    // 3、执行巡检
    ret = run(&patrol_para);
    if (ret != CAT_OK) {
        return ret;
    }

    // 4、返回巡检结果
    return get_patrol_result(cpu_set);
}

cat_return_t stop(void)
{
    set_stop_flag(true);
    return start_patrol(false);
}

cat_return_t lib_cpu_patrol_start(const char *cpumask, int cpu_utility, const int patrol_second,
    cpu_set_t *cpu_set)
{
    // 只允许一个巡检线程
    if (pthread_mutex_trylock(&g_start_mutex) != 0) {
        CAT_LOG_E("Resource busy, an cpu patrol instance is already running.");
        return CAT_ALREADY_RUNNING;
    }

    cat_return_t ret = start(cpumask, cpu_utility, patrol_second, cpu_set);

    if (pthread_mutex_unlock(&g_start_mutex) != 0) {
        CAT_LOG_E("pthread_mutex_unlock g_start_mutex failed.");
    }

    return ret;
}

cat_return_t lib_cpu_patrol_stop(void)
{
    puts("system will stop cpu patrol soon");
    // 正在停止巡检，返回成功
    if (pthread_mutex_trylock(&g_stop_mutex) != 0) {
        return CAT_OK;
    }

    cat_return_t ret = stop();

    if (pthread_mutex_unlock(&g_stop_mutex) != 0) {
        CAT_LOG_E("pthread_mutex_unlock g_stop_mutex failed.");
    }

    return ret;
}


