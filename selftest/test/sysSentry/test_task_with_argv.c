/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * sysSentry is licensed under the Mulan PSL v2.
 *
 * Description: long-running helper for the conflict=kill integration test.
 *              It accepts (and ignores) arbitrary argv so that the args appear
 *              verbatim in /proc/<pid>/cmdline; this lets check_conflict's
 *              exact-argv matching be exercised with both identical and
 *              differing command lines. It simply sleeps for a long time so
 *              the process stays alive across the test scenarios.
 * Create: 2026-09-14
 */

#include <stdio.h>
#include <unistd.h>

#define SLEEP_TIME 600

int main(int argc, char *argv[])
{
    /* args are only there to shape /proc/<pid>/cmdline; not consumed. */
    (void)argc;
    (void)argv;

    sleep(SLEEP_TIME);
    return 0;
}
