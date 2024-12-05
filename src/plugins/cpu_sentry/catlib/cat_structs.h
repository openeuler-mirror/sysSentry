#ifndef CAT_STRUCTS_H
#define CAT_STRUCTS_H
#define MAX_ERR_LEN 128

// 统一错误码
typedef enum {
    CAT_OK = 0,                 // Success
    CAT_ERR = 101,               // Error
    CAT_NOT_SUPPORTED = 102,     // The feature is not supported
    CAT_GENERIC_ERROR = 103,     // A generic, unspecified error
    CAT_LOAD_LIBRARY_FAIL = 104, // Load library fail
    CAT_ALREADY_RUNNING = 105,   // An instance is already running
    CAT_INVALID_PARAMETER = 106, // Invalid parameter
} cat_return_t;

// 巡检模块
typedef enum {
    CAT_PATROL_CPU = 0x0001,
    CAT_PATROL_MEM = 0x0002,
    CAT_PATROL_HBM = 0x0004,
    CAT_PATROL_NPU = 0x0008,
    CAT_PATROL_UNKNOWN
} cat_patrol_module;

// cli请求参数封装
typedef struct catcli_request_body {
    cat_patrol_module patrol_module;
    int patrol_second;
    int cpu_utility;
    void *module_params;
} catcli_request_body;

// 命令行选项错误信息
typedef struct option_errs {
    char patrol_module_err[MAX_ERR_LEN];
    char patrol_time_err[MAX_ERR_LEN];
    char cpulist_err[MAX_ERR_LEN];
    char cpu_usage_percentage_err[MAX_ERR_LEN];
} option_errs;
#endif

