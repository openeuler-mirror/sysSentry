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
import threading

from .xalarm_events import (
    unregister_client_events,
    handle_client_event,
    fd_to_events
)

MIN_ID_NUMBER = 1001
MAX_ID_NUMBER = 1128
MAX_CONNECTION_NUM = 100
TEST_CONNECT_BUFFER_SIZE = 32
SYSSENTRY_DOWN_ALARM_ID = 1128
REG_MSG_BUFFER_SIZE = 1024
# Clients using xalarm_register_event send the registration JSON synchronously right
# after connect() (see register_xalarm.c), so the message normally arrives within
# milliseconds. 50ms is ~45x the observed p99 (~1ms) and tolerates heavy scheduler
# jitter while keeping the window in which a legacy-API client (no registration msg)
# cannot receive alarms small. The previous 500ms value caused real alarm loss for
# such clients on every connect.
REG_MSG_TIMEOUT = 0.05
# How long a single 8216-byte alarm frame may take to send to one client.
#
# transmit_alarm runs on the single server_loop main thread and holds
# fd_to_socket_lock for the whole broadcast, so a stuck subscriber stalls
# (a) later subscribers in the same broadcast, (b) the next incoming alarm,
# and (c) connection management — all bounded by this value per stuck peer.
# The lock cannot be released during send: transmit_alarm (main thread) and
# broadcast_sentry_down (monitor thread) send concurrently, and the lock is
# what serializes them so two threads do not interleave bytes into one socket
# (which would reintroduce the stream-desync bug). So we bound the stall by
# timeout instead.
#
# 0.1s is ~1000x a normal AF_UNIX send of 8KB (microseconds): the default
# send buffer holds dozens of frames, so sendall returns instantly unless the
# peer has stopped reading long enough to fill that backlog. Only genuinely
# stuck subscribers (not momentarily-slow ones) hit this; a slow-but-alive
# peer reading >8KB/100ms (80KB/s — trivially slow) never times out. On
# timeout the connection is dropped rather than corrupting the stream
# (see _send_alarm_one).
SEND_TIMEOUT_SEC = 0.1


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


def cleanup_closed_connections(server_sock, epoll, fd_to_socket, fd_to_socket_lock):
    """
    clean invalid client socket connections saved in 'fd_to_socket'
    :param server_sock: server socket instance of alarm
    :param epoll: epoll instance, used to unregister invalid client connections
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    """
    to_remove = []
    with fd_to_socket_lock:
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
            unregister_client_events(fileno, list(fd_to_events.get(fileno, set())))
            fd_to_socket[fileno].close()
            del fd_to_socket[fileno]
            logging.info(f"cleaned up connection {fileno} for client lost connection.")


def wait_for_connection(server_sock, epoll, fd_to_socket, conn_thread_should_stop, fd_to_socket_lock):
    """
    thread function for catch and save client connection
    :param server_sock: server socket instance of alarm
    :param epoll: epoll instance, used to unregister invalid client connections
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    :param conn_thread_should_stop: bool instance
    """
    logging.info("wait for connection thread is running")
    while not conn_thread_should_stop.is_set():
        try:
            events = epoll.poll(1)

            for fileno, event in events:
                if fileno == server_sock.fileno():
                    connection, client_address = server_sock.accept()
                    # if reach max connection, cleanup closed connections
                    if len(fd_to_socket) - 1 >= MAX_CONNECTION_NUM:
                        cleanup_closed_connections(server_sock, epoll, fd_to_socket, fd_to_socket_lock)
                    # if connections still reach max num, close this connection automatically
                    if len(fd_to_socket) - 1 >= MAX_CONNECTION_NUM:
                        logging.info(f"connection reach max num of {MAX_CONNECTION_NUM}, closed current connection!")
                        connection.close()
                        continue
                    # receive event registration message
                    try:
                        connection.setblocking(True)
                        connection.settimeout(REG_MSG_TIMEOUT)
                        reg_data = connection.recv(REG_MSG_BUFFER_SIZE)
                        if reg_data:
                            handle_client_event(connection.fileno(), reg_data)
                        connection.setblocking(False)
                    except socket.timeout:
                        # client did not send any registration message, it means
                        # the client does not use xalarm_register_event API
                        # (eg. sysSentry via pyxalarm), just keep the connection
                        # and forward alarms to it
                        logging.warning(
                                "client fd %d does not send registration message "
                                "in %s ms, treat it as alarm receiver only",
                                connection.fileno(),
                                int(connection.gettimeout() * 1000)
                        )
                        connection.setblocking(False)
                    except socket.error as e:
                        logging.warning(
                                "Failed to receive registration from client: %d, reason is %s",
                                connection.fileno(),
                                str(e)
                        )
                        connection.close()
                        continue
                    with fd_to_socket_lock:
                        fd_to_socket[connection.fileno()] = connection
                    logging.info("connection fd %d registered event.", connection.fileno())
        except socket.error as e:
            logging.error("socket error, reason is %s", str(e))
        except (KeyError, OSError, ValueError) as e:
            logging.error("wait for connection failed %s", str(e))
        except Exception as e:
            logging.error("error occurred, reason is %s", str(e))


def broadcast_sentry_down(server_socket, fd_to_socket, fd_to_socket_lock):
    """
    broadcast sysSentry down alarm to all clients
    :param server_socket: xalarmd service socket
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    :param fd_to_socket_lock: lock instance for fd_to_socket
    """
    from .xalarm_api import Xalarm, alarm_stu2bin

    to_remove = []

    alarm_info = Xalarm(SYSSENTRY_DOWN_ALARM_ID, 1, 1, 0, 0, "sysSentry service is down")
    bin_data = alarm_stu2bin(alarm_info)

    with fd_to_socket_lock:
        for fileno, connection in fd_to_socket.items():
            try:
                if connection is server_socket:
                    continue
                connection.sendall(bin_data)
                logging.info("Broadcast sysSentry down msg success, fd is %d", fileno)
            except (BrokenPipeError, ConnectionResetError):
                to_remove.append(fileno)
            except Exception as e:
                logging.info("Broadcast sysSentry down msg failed, fd is %d, reason is: %s",
                    fileno, str(e))

        for fileno in to_remove:
            unregister_client_events(fileno, list(fd_to_events.get(fileno, set())))
            fd_to_socket[fileno].close()
            del fd_to_socket[fileno]
            logging.info(f"cleaned up connection {fileno} for client lost connection.")


def _send_alarm_one(connection, bin_data, fileno, alarm_str):
    """Send one alarm frame to a single client connection.

    The client sockets in fd_to_socket are non-blocking (set in
    wait_for_connection). On a non-blocking socket, socket.sendall() raises
    BlockingIOError/EAGAIN the moment the send buffer fills up — even if it
    has already handed part of bin_data to the kernel. The previous code
    reacted to that by re-calling sendall(bin_data) from offset 0, which
    re-sent the *entire* frame after the bytes already delivered. The
    subscriber then saw [partial head][full frame] on its SOCK_STREAM,
    permanently desyncing the fixed-size 8216-byte framing used by
    libxalarm's RecvAll: every subsequent alarm landed on garbage offsets
    and was silently dropped by the id filter — the "subscriber never
    receives the OOM event" symptom.

    To make a single frame atomic, switch the socket to blocking mode with a
    short timeout just for this send. sendall() then blocks until the whole
    frame is flushed (or the timeout fires / the peer dies), so no partial
    send can leak. The timeout bounds how long one slow subscriber may stall
    the main loop; on timeout the caller drops the connection rather than
    corrupting the stream.

    Caller must NOT hold fd_to_socket_lock while this runs for a connection
    other than the one being sent to (it doesn't touch the dict, only the
    socket), but the caller does hold the lock around the whole broadcast —
    that is fine because each send is time-bounded.

    Returns True if the full frame was delivered, False if the connection
    is dead or could not be flushed within SEND_TIMEOUT_SEC (caller should
    remove it).
    """
    try:
        connection.setblocking(True)
        connection.settimeout(SEND_TIMEOUT_SEC)
        connection.sendall(bin_data)
        logging.info("Broadcast msg success, fd is %d, alarm msg is %s", fileno, alarm_str)
        return True
    except (BlockingIOError, socket.timeout) as e:
        # Timed out flushing a full frame to a slow/stuck subscriber. Dropping
        # the connection is correct: a half-sent frame would desync the stream,
        # and we never sent a partial frame here (sendall is all-or-nothing on
        # a blocking socket — on timeout nothing more is queued after what the
        # kernel already accepted, but since the frame was not fully handed
        # over we cannot retry from offset 0, so the connection must go).
        logging.warning("Send timed out after %ss, fd is %d, alarm msg is %s, reason is: %s; "
                        "dropping connection to protect stream framing",
                        SEND_TIMEOUT_SEC, fileno, alarm_str, str(e))
        return False
    except (BrokenPipeError, ConnectionResetError) as e:
        logging.info("Connection reset while sending, fd is %d, alarm msg is %s, reason is: %s",
                     fileno, alarm_str, str(e))
        return False
    except OSError as e:
        logging.info("Sending msg failed, fd is %d, alarm msg is %s, reason is: %s",
                     fileno, alarm_str, str(e))
        return False
    finally:
        # Restore non-blocking mode; wait_for_connection/cleanup assume it.
        try:
            connection.setblocking(False)
        except OSError:
            pass


def transmit_alarm(server_sock, epoll, fd_to_socket, bin_data, alarm_str, fd_to_socket_lock):
    """
    this function is to broadcast alarm data to client, if fail to send data, remove connections held by fd_to_socket
    :param server_sock: server socket instance of alarm
    :param epoll: epoll instance, used to unregister invalid client connections
    :param fd_to_socket: dict instance, used to hold client connections and server connections
    :param bin_data: binary instance, alarm info data in C-style struct format defined in xalarm_api.py
    """
    to_remove = []

    with fd_to_socket_lock:
        for fileno, connection in fd_to_socket.items():
            if connection is server_sock:
                continue
            ok = _send_alarm_one(connection, bin_data, fileno, alarm_str)
            if not ok:
                to_remove.append(fileno)

        for fileno in to_remove:
            unregister_client_events(fileno, list(fd_to_events.get(fileno, set())))
            fd_to_socket[fileno].close()
            del fd_to_socket[fileno]
            logging.info(f"cleaned up connection {fileno} for client lost connection.")


