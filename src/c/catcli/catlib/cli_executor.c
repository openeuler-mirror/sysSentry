#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include "cpu_patrol.h"
#include "cli_executor.h"

static void print_cpu_set(cpu_set_t *cpu_set, ssize_t num_cpus, const char *prefix)
{
    bool is_begin_set = false;
    int begin_cpuid = -1;
    int end_cpuid = -1;
    printf(prefix);
    for (ssize_t i = 0; i < num_cpus; i++) {
        if (!CPU_ISSET(i, cpu_set)) {
            continue;
        }
        if (!is_begin_set) {
            is_begin_set = true;
            begin_cpuid = i;
            end_cpuid = begin_cpuid;
            continue;
        }
        if (i == (end_cpuid + 1)) {
            end_cpuid++;
            continue;
        }

        if (begin_cpuid == end_cpuid) {
            printf("%u,", begin_cpuid);
        } else {
            printf("%u-%u,", begin_cpuid, end_cpuid);
        }

        end_cpuid = i;
        begin_cpuid = end_cpuid;
    }
    if (begin_cpuid == end_cpuid) {
        printf("%u\n", begin_cpuid);
    } else {
        printf("%u-%u\n", begin_cpuid, end_cpuid);
    }
}

int execute_request(struct catcli_request_body *request_body)
{
    cat_patrol_module module = request_body->patrol_module;
    if (module == CAT_PATROL_CPU) {
        cpu_set_t *cpu_set = __CPU_ALLOC(MAX_CPU_LOGIC_CORE);
        ssize_t size = __CPU_ALLOC_SIZE(MAX_CPU_LOGIC_CORE);
        __CPU_ZERO_S(size, cpu_set);
        int ret = lib_cpu_patrol_start(request_body->module_params, request_body->cpu_utility,
            request_body->patrol_second, cpu_set);
        int count = __CPU_COUNT_S(size, cpu_set);
        printf("cpu patrol execute %s, isolated cores: %d\n", ret == CAT_OK ? "ok" : "failed", count);
        char *prefix = "<ISOLATED-CORE-LIST>:";
        if (count == 0) {
            puts(prefix);
        } else {
            print_cpu_set(cpu_set, MAX_CPU_LOGIC_CORE, prefix);
        }
        __CPU_FREE(cpu_set);
        return ret;
    } else {
        puts("Only CPU Patrol is supported currently!");
        return CAT_NOT_SUPPORTED;
    }
}


