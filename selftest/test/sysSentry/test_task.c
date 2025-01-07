
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: test program
 * Author: zhuo feng
 * Create: 2023-11-11
 */

#include <stdio.h>
#include <unistd.h>

#define SLEEP_TIME 10

int main(void)
{
    printf("-----test begin-----\n");
    sleep(SLEEP_TIME);
    printf("-----test end  -----\n");

    return 0;
}