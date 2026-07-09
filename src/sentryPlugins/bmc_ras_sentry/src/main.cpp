/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * bmc_ras_sentry is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * Author: hewanhan@h-partners.com
 */

#include <iostream>
#include <csignal>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include "cbmcrassentry.h"
#include "configure.h"
#include "logger.h"
#include "common.h"

std::atomic<bool> g_exit(false);
std::mutex g_mutex;
std::condition_variable g_cv;

const std::string TOOL_NAME = "bmc_ras_sentry";
const std::string PID_FILE_PATH = "/var/run/" + TOOL_NAME + ".pid";

void HandleSignal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT) {
        g_exit = true;
        BMC_LOG_INFO << "Receive signal SIGTERM or SIGINT, exit.";
        g_cv.notify_all();
    }
    return;
}

void SetSignalHandler()
{
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);  // ras SIGTERM
    sigaddset(&sa.sa_mask, SIGINT);   // ras SIGINT
    sa.sa_handler = HandleSignal;
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        BMC_LOG_ERROR << "Failed to setup signal with SIGTERM:" << strerror(errno);
    }

    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        BMC_LOG_ERROR << "Failed to setup signal with SIGINT:" << strerror(errno);
    }
}

static int handle_file_lock(int fd, bool lock)
{
    int ret;
    struct flock fl;
    fl.l_type   = lock ? F_WRLCK : F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;

    ret = fcntl(fd, F_SETLK, &fl);
    if (ret < 0) {
        BMC_LOG_ERROR << "fcntl failed, error msg is " << strerror(errno);
    } else {
        BMC_LOG_DEBUG << "fcntl success, lock ret code is " << ret;
    }
    return ret;
}

static int check_and_set_pid_file(void)
{
    int ret, fd;
    fd = open(PID_FILE_PATH.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        BMC_LOG_ERROR << "open file " << PID_FILE_PATH << " failed!";
        return -1;
    }

    ret = handle_file_lock(fd, true);
    if (ret < 0) {
        BMC_LOG_ERROR << TOOL_NAME << " is already running";
        close(fd);
        return ret;
    }

    return fd;
}

static int release_pid_file(int fd)
{
    int ret;
    ret = handle_file_lock(fd, false);
    if (ret < 0) {
        close(fd);
        BMC_LOG_ERROR << "release pid file " << PID_FILE_PATH << " lock failed, error msg is " << strerror(errno);
        return ret;
    }

    close(fd);
    ret = remove(PID_FILE_PATH.c_str());
    if (ret < 0) {
        BMC_LOG_ERROR << "remove " << PID_FILE_PATH << " failed, error msg is " << strerror(errno);
    }
    return ret;
}

int main(int argc, char* argv[])
{
    int pid_fd;

    if (!BMCRasSentryPlu::Logger::GetInstance().Initialize(BMCRasSentryPlu::BMCPLU_LOG_PATH)) {
        std::cerr << "Failed to initialize logger." << std::endl;
    }
    SetSignalHandler();

    pid_fd = check_and_set_pid_file();
    if (pid_fd < 0) {
        return pid_fd;
    }

    BMCRasSentryPlu::CBMCRasSentry ras_sentry;
    PluConfig config;
    if (BMCRasSentryPlu::ParseConfig(BMCRasSentryPlu::BMCPLU_CONFIG_PATH, config)) {
        BMC_LOG_ERROR << "Parse config failed, plugin exit.";
        release_pid_file(pid_fd);
        return -1;
    } else {
        BMCRasSentryPlu::Logger::GetInstance().SetLevel(config.logLevel);
        ras_sentry.SetPatrolInterval(config.patrolSeconds);
        ras_sentry.SetBMCEvents(config.BMCEvents);
    }

    std::thread configMonitor([&] {
        time_t lastModTime = 0;
        struct stat st;
        if (stat(BMCRasSentryPlu::BMCPLU_CONFIG_PATH.c_str(), &st) == 0) {
            lastModTime = st.st_mtime;
        }

        while (!g_exit) {
            {
                std::unique_lock<std::mutex> lock(g_mutex);
                g_cv.wait_for(lock, std::chrono::seconds(BMCRasSentryPlu::BMCPLU_CONFIG_CHECK_CYCLE),
                              [] { return g_exit.load(); });
                if (g_exit) {
                    break;
                }
            }

            struct stat st_;
            if (stat(BMCRasSentryPlu::BMCPLU_CONFIG_PATH.c_str(), &st_) != 0) {
                continue;
            }
            if (st_.st_mtime != lastModTime) {
                lastModTime = st_.st_mtime;
                PluConfig newConfig;
                if (BMCRasSentryPlu::ParseConfig(BMCRasSentryPlu::BMCPLU_CONFIG_PATH, newConfig) == BMCPLU_SUCCESS) {
                    if (newConfig.logLevel != config.logLevel) {
                        config.logLevel = newConfig.logLevel;
                        BMC_LOG_INFO << "Log level update to "
                                     << BMCRasSentryPlu::Logger::GetInstance().LevelToString(config.logLevel);
                        BMCRasSentryPlu::Logger::GetInstance().SetLevel(config.logLevel);
                    }
                    if (newConfig.patrolSeconds != config.patrolSeconds) {
                        config.patrolSeconds = newConfig.patrolSeconds;
                        BMC_LOG_INFO << "Patrol interval update to " << config.patrolSeconds;
                        ras_sentry.SetPatrolInterval(config.patrolSeconds);
                    }
                    if (newConfig.BMCEvents != config.BMCEvents) {
                        config.BMCEvents = newConfig.BMCEvents;
                        for (const auto& event : config.BMCEvents) {
                            BMC_LOG_INFO << "BMC Event update to " << event;
                        }
                        ras_sentry.SetBMCEvents(config.BMCEvents);
                    }
                }
            }
        }
    });

    ras_sentry.Start();
    while (!g_exit) {
        {
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait_for(lock, std::chrono::seconds(BMCRasSentryPlu::BMCPLU_DEFAULT_SLEEP_CYCLE),
                          [] { return g_exit.load(); });
        }
        if (!ras_sentry.IsRunning()) {
            g_exit = true;
            g_cv.notify_all();
            break;
        }
    }
    ras_sentry.Stop();
    if (configMonitor.joinable()) {
        configMonitor.join();
    }

    release_pid_file(pid_fd);
    return 0;
}
