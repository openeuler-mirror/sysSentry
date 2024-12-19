#ifndef CPU_PATROL_H
#define CPU_PATROL_H

#include <sched.h>
#include "cat_structs.h"
#define MAX_PATROL_TIMES 36000         // 巡检用例执行的次数，最佳效果为不低于36000
#define PATROL_TIMES_PER_ITERATION 200 // 把MAX_PATROL_TIMES分成多次迭代进行，每次迭代内核执行的巡检用例次数，CPU利用率100%情况下，一次迭代执行约1秒，防呆
#define DECIMAL_STR_LEN 11
#define DEFAULT_CPU_UTILITY 50  // 内核巡检进程默认CPU利用率
#define MIN_CPU_UTILITY 1       // 内核巡检进程最小CPU利用率
#define MAX_CPU_UTILITY 100     // 内核巡检进程最大CPU利用率
#define MAX_CPU_LOGIC_CORE 1024 // 最大CPU逻辑核数
#define KERNEL_INTF_PATH_CPUMASK "/sys/devices/system/cpu/cpuinspect/cpumask"
#define KERNEL_INTF_PATH_PATROL_TIMES "/sys/devices/system/cpu/cpuinspect/patrol_times"
#define KERNEL_INTF_PATH_CPU_UTILITY "/sys/devices/system/cpu/cpuinspect/cpu_utility"

typedef struct {
    const char *cpumask;        // cpu core list, eg. 0-7,9-127
    unsigned int cpu_utility;   // 内核巡检进程最大cpu利用率
    unsigned int patrol_times;  // 内核执行的CPU巡检用例次数
    unsigned int patrol_second; // 持续巡检时间(秒)
} cpu_patrol_para;

/*
 * input: cpumask: 巡检的CPU核范围，格式"55,32,16,1-3,13-17"；
 * cpu_utility: 内核巡检进程最大CPU利用率，范围[1-100]，若输入值非法，则取默认值DEFAULT_CPU_UTILITY}
 * patrol_time: 巡检持续时间(秒),大于1整数
 * output: cpu_set：返回巡检被隔离的核列表
 */
cat_return_t lib_cpu_patrol_start(const char *cpumask, int cpu_utility, int patrol_time,
    cpu_set_t *cpu_set);

/*
 * 停止巡检
 */
cat_return_t lib_cpu_patrol_stop(void);


#endif

