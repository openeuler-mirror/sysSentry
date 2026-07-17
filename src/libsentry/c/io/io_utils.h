/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: I/O utils for sysSentry — handles partial send/recv/write
 * Author: sxt1001
 * Create: 2026-07-20
 */

#ifndef SYSSENTRY_IO_UTILS_H
#define SYSSENTRY_IO_UTILS_H

#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>

#define RECV_DELAY_MSEC 100
#define TIME_UNIT_MILLISECONDS 1000

/**
 * @brief Write all data to a file descriptor, handling partial writes.
 *
 * On stream file descriptors, write() may return fewer bytes than
 * requested. This function loops until all data is written or an
 * unrecoverable error occurs.
 *
 * @param fd    file descriptor
 * @param buf   data to write
 * @param count total number of bytes to write
 * @return      total bytes written on success, or -1 on error (errno set)
 */
ssize_t WriteAll(int fd, const char *buf, size_t count);

/**
 * @brief Send all data through a stream socket, handling partial sends.
 *
 * On stream sockets (SOCK_STREAM), send() may return fewer bytes than
 * requested. This function loops until all data is sent or an
 * unrecoverable error occurs.
 *
 * @param fd    socket file descriptor
 * @param buf   data to send
 * @param len   total number of bytes to send
 * @return      total bytes sent on success, or -1 on error (errno set)
 */
ssize_t SendAll(int fd, const char *buf, size_t len);

/**
 * @brief Receive exactly `len` bytes from a stream socket.
 *
 * On stream sockets, recv() may return fewer bytes than requested.
 * This function loops until exactly `len` bytes are received, the
 * connection is closed, or an unrecoverable error occurs.
 *
 * @param fd    socket file descriptor
 * @param buf   buffer to receive data into
 * @param len   exact number of bytes to receive
 * @return      total bytes received on success, 0 if connection closed
 *              by peer before any data, or -1 on error (errno set)
 */
ssize_t RecvAll(int fd, char *buf, size_t len);

#endif
