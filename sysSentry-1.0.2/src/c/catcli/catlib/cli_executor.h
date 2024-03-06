#ifndef CATHELPER_CLI_EXECUTOR_H
#define CATHELPER_CLI_EXECUTOR_H

#include "cat_structs.h"

/**
 * 执行cli请求
 * @param request_body 封装的请求参数
 * @return
 */
int execute_request(struct catcli_request_body *request_body);

#endif // CATHELPER_CLI_EXECUTOR_H

