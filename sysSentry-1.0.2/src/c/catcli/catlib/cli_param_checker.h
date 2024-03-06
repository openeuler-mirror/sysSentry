#ifndef CATHELPER_CLI_PARAM_CHECKER_H
#define CATHELPER_CLI_PARAM_CHECKER_H

#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <regex.h>
#include <stdbool.h>
#include "cli_common.h"
#include "cat_structs.h"
/**
 * 检查cpu使用率百分比 (0,100]
 * @param optarg
 * @param p_request_body
 * @param errs
 */
void checkset_cpu_usage_percentage(char *getopt_optarg, catcli_request_body *p_request_body, struct option_errs *errs);

/**
 * 检查cpulist,正确格式如:1,3-7
 * @param optarg
 * @param p_request_body
 * @param errs
 */
void checkset_cpulist(char *getopt_optarg, catcli_request_body *p_request_body, struct option_errs *errs);

/**
 * 检查巡检时间，大于0整数，秒
 * @param optarg
 * @param p_request_body
 * @param errs
 */
void checkset_patrol_time(char *getopt_optarg, catcli_request_body *p_request_body, struct option_errs *errs);

/**
 * 检查巡检类型 0x0001(CPU)|0x0002(MEM)|0x0004(HBM)|0x0008(NPU)
 * @param optarg
 * @param p_request_body
 * @param errs
 */
void checkset_patrol_type(char *getopt_optarg, catcli_request_body *p_request_body, struct option_errs *errs);

/**
 * 参数间依赖检查
 * @param p_request_body
 * @param p_option_errs
 * @return
 */
int checkParamsDependency(catcli_request_body *p_request_body, option_errs *p_option_errs);

#endif // CATHELPER_CLI_PARAM_CHECKER_H


