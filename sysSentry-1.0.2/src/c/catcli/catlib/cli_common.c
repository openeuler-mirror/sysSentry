#include <stdio.h>
#include "cli_common.h"

void print_err_help()
{
    printf("See 'cat-cli ");
    PRINT_BLUE("--help");
    printf("'\n");
}

void print_opts_help()
{
    printf("usage: cat-cli [OPTIONS]\n"
        "\n"
        "Options:\n"
        "-h, --help                                                     :Show the help message\n"
        "-m, --patrol_module                                            :0x0001(CPU)|0x0002(MEM)|0x0004(HBM)|0x0008(NPU)\n"
        "-t, --patrol_second                                            :Patrol time(second),an integer greater than 0\n"
        "-l, --cpulist                                                  :Specify patrol cpu IDs,\"-l\" is valid when \"-m 0x0001\",eg:\"-l 0-3,7\"\n"
        "-u, --cpu_utility                                              :The maximum CPU usage time percentage of the patrol program,the range is (0,100],default:50\n");
}

