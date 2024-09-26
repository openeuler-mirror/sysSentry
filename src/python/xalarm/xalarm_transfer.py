# coding: utf-8
# Copyright (c) 2023 Huawei Technologies Co., Ltd.
# sysSentry is licensed under the Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#     http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# PURPOSE.
# See the Mulan PSL v2 for more details.
"""
Description: xalarm transfer
Author:
Create: 2023-11-02
"""

import socket
import logging
import select

MIN_ID_NUMBER = 1001
MAX_ID_NUMBER = 1128
MAX_CONNECTION_NUM = 100 
TEST_CONNECT_BUFFER_SIZE = 32


def check_filter(alarm_info, alarm_filter):
    """check id_mask for alarm messages forwarding
    """
    if not alarm_filter:
        return True
    if alarm_info.alarm_id < MIN_ID_NUMBER or alarm_info.alarm_id > MAX_ID_NUMBER:
        return False
    index = alarm_info.alarm_id - MIN_ID_NUMBER
    if not alarm_filter.id_mask[index]:
        return False
    return True


def cleanup_closed_connections(server_sock, epoll, fd_to_socket):
    """
    clean invalid client socket connections saved in 'fd_to_socket'
    :param server_sock: server socket instance of alarm
    :param epoll: epoll instance, used to unregister invalid client connections
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    """
    to_remove = []
    for fileno, connection in fd_to_socket.items():
        if connection is server_sock:
            continue
        try:
            # test whether connection still alive, use MSG_DONTWAIT to avoid blocking thread
            # use MSG_PEEK to avoid consuming buffer data
            data = connection.recv(TEST_CONNECT_BUFFER_SIZE, socket.MSG_DONTWAIT | socket.MSG_PEEK)
            if not data:
                to_remove.append(fileno)
        except BlockingIOError:
            pass
        except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
            to_remove.append(fileno)
    
    for fileno in to_remove:
        epoll.unregister(fileno)
        fd_to_socket[fileno].close()
        del fd_to_socket[fileno]
        logging.info(f"cleaned up connection {fileno} for client lost connection.")


def wait_for_connection(server_sock, epoll, fd_to_socket, thread_should_stop):
    """
    thread function for catch and save client connection
    :param server_sock: server socket instance of alarm
    :param epoll: epoll instance, used to unregister invalid client connections
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    :param thread_should_stop: bool instance
    """
    while not thread_should_stop:
        try:
            events = epoll.poll(1)
            
            for fileno, event in events:
                if fileno == server_sock.fileno():
                    connection, client_address = server_sock.accept()
                    # if reach max connection, cleanup closed connections
                    if len(fd_to_socket) - 1 >= MAX_CONNECTION_NUM:
                        cleanup_closed_connections(server_sock, epoll, fd_to_socket)
                    # if connections still reach max num, close this connection automatically
                    if len(fd_to_socket) - 1 >= MAX_CONNECTION_NUM:
                        logging.info(f"connection reach max num of {MAX_CONNECTION_NUM}, closed current connection!")
                        connection.close()
                        continue
                    epoll.register(connection.fileno(), select.EPOLLOUT)
                    fd_to_socket[connection.fileno()] = connection
        except socket.error as e: 
            logging.debug(f"socket error, reason is {e}")
            break
        except (KeyError, OSError, ValueError) as e:
            logging.debug(f"wait for connection failed {e}")


def transmit_alarm(server_sock, epoll, fd_to_socket, bin_data):
    """
    this function is to broadcast alarm data to client, if fail to send data, remove connections held by fd_to_socket
    :param server_sock: server socket instance of alarm
    :param epoll: epoll instance, used to unregister invalid client connections
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    :param bin_data: binary instance, alarm info data in C-style struct format defined in xalarm_api.py
    """
    to_remove = []
    for fileno, connection in fd_to_socket.items():
        if connection is not server_sock:
            try:
                connection.sendall(bin_data)
            except (BrokenPipeError, ConnectionResetError):
                to_remove.append(fileno)
    for fileno in to_remove:
        epoll.unregister(fileno)
        fd_to_socket[fileno].close()
        del fd_to_socket[fileno]

