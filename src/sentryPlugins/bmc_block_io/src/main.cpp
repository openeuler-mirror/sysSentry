/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * bmc_block_io is licensed under Mulan PSL v2.
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
#include "cbmcblockio.h"
#include "configure.h"
#include "logger.h"
#include "common.h"

std::atomic<bool> g_exit(false);
std::mutex g_mutex;
std::condition_variable g_cv;

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
    sigaddset(&sa.sa_mask, SIGTERM);  // block SIGTERM
    sigaddset(&sa.sa_mask, SIGINT);   // block SIGINT
    sa.sa_handler = HandleSignal;
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        BMC_LOG_ERROR << "Failed to setup signal with SIGTERM:" << strerror(errno);
    }

    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        BMC_LOG_ERROR << "Failed to setup signal with SIGINT:" << strerror(errno);
    }
}

int main(int argc, char* argv[])
{
    if (!BMCBlockIoPlu::Logger::GetInstance().Initialize(BMCBlockIoPlu::BMCPLU_LOG_PATH)) {
        std::cerr << "Failed to initialize logger." << std::endl;
    }
    SetSignalHandler();

    BMCBlockIoPlu::CBMCBlockIo blockIo;
    PluConfig config;
    if (BMCBlockIoPlu::ParseConfig(BMCBlockIoPlu::BMCPLU_CONFIG_PATH, config)) {
        BMC_LOG_ERROR << "Parse config failed, use default configuration.";
    } else {
        BMCBlockIoPlu::Logger::GetInstance().SetLevel(config.logLevel);
        blockIo.SetPatrolInterval(config.patrolSeconds);
    }

    std::thread configMonitor([&] {
        time_t lastModTime = 0;
        struct stat st;
        if (stat(BMCBlockIoPlu::BMCPLU_CONFIG_PATH.c_str(), &st) == 0) {
            lastModTime = st.st_mtime;
        }

        while (!g_exit) {
            {
                std::unique_lock<std::mutex> lock(g_mutex);
                g_cv.wait_for(lock, std::chrono::seconds(BMCBlockIoPlu::BMCPLU_CONFIG_CHECK_CYCLE),
                              [] { return g_exit.load(); });
                if (g_exit) {
                    break;
                }
            }

            struct stat st_;
            if (stat(BMCBlockIoPlu::BMCPLU_CONFIG_PATH.c_str(), &st_) != 0) {
                continue;
            }
            if (st_.st_mtime != lastModTime) {
                lastModTime = st_.st_mtime;
                PluConfig newConfig;
                if (BMCBlockIoPlu::ParseConfig(BMCBlockIoPlu::BMCPLU_CONFIG_PATH, newConfig) == BMCPLU_SUCCESS) {
                    if (newConfig.logLevel != config.logLevel) {
                        config.logLevel = newConfig.logLevel;
                        BMC_LOG_INFO << "Log level update to "
                                     << BMCBlockIoPlu::Logger::GetInstance().LevelToString(config.logLevel);
                        BMCBlockIoPlu::Logger::GetInstance().SetLevel(config.logLevel);
                    }
                    if (newConfig.patrolSeconds != config.patrolSeconds) {
                        config.patrolSeconds = newConfig.patrolSeconds;
                        BMC_LOG_INFO << "Patrol interval update to " << config.patrolSeconds;
                        blockIo.SetPatrolInterval(config.patrolSeconds);
                    }
                }
            }
        }
    });
    blockIo.Start();
    while (!g_exit) {
        {
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait_for(lock, std::chrono::seconds(BMCBlockIoPlu::BMCPLU_DEFAULT_SLEEP_CYCLE),
                          [] { return g_exit.load(); });
        }
        if (!blockIo.IsRunning()) {
            g_exit = true;
            g_cv.notify_all();
            break;
        }
    }
    blockIo.Stop();
    if (configMonitor.joinable()) {
        configMonitor.join();
    }
    return 0;
}
