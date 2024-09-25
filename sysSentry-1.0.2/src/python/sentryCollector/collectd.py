# coding: utf-8
# Copyright (c) 2024 Huawei Technologies Co., Ltd.
# sysSentry is licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.

"""
main loop for collect.
"""
import sys
import signal
import traceback
import socket
import os
import json
import logging
import fcntl
import select

import threading

from .collect_io import CollectIo
from .collect_server import CollectServer
from .collect_config import CollectConfig

SENTRY_RUN_DIR = "/var/run/sysSentry"
COLLECT_SOCKET_PATH = "/var/run/sysSentry/collector.sock"
SENTRY_RUN_DIR_PERM = 0o750

COLLECT_LOG_FILE = "/var/log/sysSentry/collector.log"
Thread_List = []
Module_Map_Class = {"io" : CollectIo}

def remove_sock_file():
    try:
        os.unlink(COLLECT_SOCKET_PATH)
    except FileNotFoundError:
        pass

def sig_handler(signum, _f):
    if signum not in (signal.SIGINT, signal.SIGTERM):
        return
    for i in range(len(Thread_List)):
        Thread_List[i][0].stop_thread()

    remove_sock_file()

def main():
    """main
    """
    if not os.path.exists(SENTRY_RUN_DIR):
        os.mkdir(SENTRY_RUN_DIR)
        os.chmod(SENTRY_RUN_DIR, mode=SENTRY_RUN_DIR_PERM)

    logging.basicConfig(filename=COLLECT_LOG_FILE, level=logging.INFO, 
        format='%(asctime)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    os.chmod(COLLECT_LOG_FILE, 0o600)

    try:
        signal.signal(signal.SIGINT, sig_handler)
        signal.signal(signal.SIGTERM, sig_handler)

        logging.info("finish main parse_args")

        module_config = CollectConfig()
        module_list = module_config.modules

        # listen thread
        cs = CollectServer()
        listen_thread = threading.Thread(target=cs.server_loop)
        listen_thread.start()
        Thread_List.append([cs, listen_thread])

        # collect thread
        for info in module_list:
            class_name = Module_Map_Class.get(info)
            if not class_name:
                logging.info("%s correspond to class is not exist", info)
                continue
            cn = class_name(module_config)
            collect_thread = threading.Thread(target=cn.main_loop)
            collect_thread.start()
            Thread_List.append([cn, collect_thread])

        for i in range(len(Thread_List)):
            Thread_List[i][1].join()
            
    except Exception:
        logging.error('%s', traceback.format_exc())
    finally:
        pass

    logging.info("all threads have finished. main thread exit.")