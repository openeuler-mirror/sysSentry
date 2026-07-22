/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: I/O utils for sysSentry — handles partial send/recv/write
 * Author: sxt1001
 * Create: 2026-07-20
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "io_utils.h"

ssize_t WriteAll(int fd, const char *buf, size_t count)
{
    const char *ptr = buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t ret = write(fd, ptr, remaining);
        if (ret < 0) {
            /* EINTR means recv func interrupted by signal
             * EAGAIN and EWOULDBLOCK means recv has been blocked(in nonblock mode)
             */
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(RECV_DELAY_MSEC * TIME_UNIT_MILLISECONDS);
                continue;
            }
            return -1;
        }
        if (ret == 0) {
            /* zero-write — not recoverable */
            errno = EIO;
            return -1;
        }
        ptr += ret;
        remaining -= ret;
    }

    return (ssize_t)count;
}

ssize_t SendAll(int fd, const char *buf, size_t len)
{
    const char *p = buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t ret = send(fd, p, remaining, 0);
        if (ret < 0) {
            /* EINTR means recv func interrupted by signal
             * EAGAIN and EWOULDBLOCK means recv has been blocked(in nonblock mode)
             */
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(RECV_DELAY_MSEC * TIME_UNIT_MILLISECONDS);
                continue;
            }
            return -1;
        }
        if (ret == 0) {
            /* connection closed unexpectedly */
            errno = EPIPE;
            return -1;
        }
        p += ret;
        remaining -= ret;
    }

    return (ssize_t)len;
}

ssize_t RecvAll(int fd, char *buf, size_t len)
{
    char *p = buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t ret = recv(fd, p, remaining, 0);
        if (ret < 0) {
            /* EINTR means recv func interrupted by signal
             * EAGAIN and EWOULDBLOCK means recv has been blocked(in nonblock mode)
             */
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(RECV_DELAY_MSEC * TIME_UNIT_MILLISECONDS);
                continue;
            }
            return -1;
        }
        if (ret == 0) {
            /* connection closed by peer */
            if (remaining == len) {
                /* no data received at all — peer closed cleanly */
                return 0;
            }
            /* partial data received then peer closed — treat as error */
            errno = ECONNRESET;
            return -1;
        }
        p += ret;
        remaining -= ret;
    }

    return (ssize_t)len;
}
