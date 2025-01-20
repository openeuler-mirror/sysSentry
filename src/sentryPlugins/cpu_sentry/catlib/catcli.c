#include <getopt.h>
#include "cat_structs.h"
#include "cli_common.h"
#include "cat_structs.h"
#include "cli_param_checker.h"
#include "cli_executor.h"


int main(int argc, char * const argv[])
{
    if (argc <= 1) {
        print_err_help();
        return CAT_ERR;
    }
    int opt = 0;
    struct option long_opts[] = {HELP, PATROL_MODULE, PATROL_TIME, CPUS, CPU_USAGE_PERCENTAGE, END_FLAG};
    char *short_opts = "hm:t:l:u:";
    catcli_request_body request_body = {
        .module_params = NULL,
        .cpu_utility = 50,
        .patrol_module = CAT_PATROL_UNKNOWN,
        .patrol_second = 0
    };
    option_errs option_errs = { 0 };
    while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch ((char)opt) {
            case 'h':
                print_opts_help();
                return CAT_OK;
            case 'm':
                checkset_patrol_type(optarg, &request_body, &option_errs);
                break;
            case 't':
                checkset_patrol_time(optarg, &request_body, &option_errs);
                break;
            case 'l':
                checkset_cpulist(optarg, &request_body, &option_errs);
                break;
            case 'u':
                checkset_cpu_usage_percentage(optarg, &request_body, &option_errs);
                break;
            default:
                print_err_help();
                return CAT_ERR;
        }
    }
    int ret = checkParamsDependency(&request_body, &option_errs);
    RETURN_NOT_TRUE(ret == CAT_OK, NULL, NULL); // 检查到的参数值有异常不再后续检查
    return execute_request(&request_body);
}


