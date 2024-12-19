#ifndef CATHELPER_CLI_COMMON_H
#define CATHELPER_CLI_COMMON_H

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#define DECIMAL 10 /* 十进制 */

// 期望bool_expression为true，否则return CAT_ERR，(bool_expression可能为表达式！！！)
#define RETURN_NOT_TRUE(bool_expression, to_release, fail_msg, ...) \
    do {                                                            \
        if (!(bool_expression)) {                                   \
            if (fail_msg != NULL) {                                 \
                printf(fail_msg, ##__VA_ARGS__), printf("\n");      \
            }                                                       \
            free(to_release);                                       \
            return CAT_ERR;                                         \
        }                                                           \
    } while (0)

// 红色打印
#define PRINT_RED(msg, ...)          \
    do {                             \
        fprintf(stdout, "\033[31m"); \
        printf(msg, ##__VA_ARGS__);  \
        fprintf(stdout, "\033[0m");  \
    } while (0)
// 蓝色打印
#define PRINT_BLUE(msg, ...)              \
    do {                                  \
        fprintf(stdout, "\033[0;32;34m"); \
        printf(msg, ##__VA_ARGS__);       \
        fprintf(stdout, "\033[0m");       \
    } while (0)

// 绿色打印
#define PRINT_GREEN(msg, ...)             \
    do {                                  \
        fprintf(stdout, "\033[0;32;32m"); \
        printf(msg, ##__VA_ARGS__);       \
        fprintf(stdout, "\033[0m");       \
    } while (0)


static const struct option HELP = { "help", no_argument, NULL, 'h' };
static const struct option PATROL_MODULE = { "patrol_module", required_argument, NULL, 'm' };
static const struct option PATROL_TIME = { "patrol_second", required_argument, NULL, 't' };
static const struct option CPUS = { "cpulist", required_argument, NULL, 'l' };
static const struct option CPU_USAGE_PERCENTAGE = {"cpu_utility", required_argument, NULL, 'u' };
static const struct option END_FLAG = { 0, 0, 0, 0 }; // 避免长选项未匹配到时发生段错误

/**
 * 打印错误help提示
 */
void print_err_help();

/**
 * 打印命令行选项帮助信息
 */
void print_opts_help();

#endif // CATHELPER_CLI_COMMON_H


