/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * bmc_block_io is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * Author: hewanhan@h-partners.com
 */

#ifndef _BMC_BLOCK_IO_H_
#define _BMC_BLOCK_IO_H_

#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <cctype>
#include <set>
#include <condition_variable>

namespace BMCBlockIoPlu {

struct ResponseHeader {
    uint16_t totalEvents;
    uint8_t eventCount;
    bool valid;
};

struct IPMIEvent {
    uint32_t alarmTypeCode;
    uint32_t timestamp;
    uint8_t severity;
    uint8_t subjectType;
    uint8_t deviceId;
    bool valid;
};

class CBMCBlockIo {
public:
    CBMCBlockIo();
    ~CBMCBlockIo();
    void Start();
    void Stop();
    void SetPatrolInterval(int seconds);
    bool IsRunning();
private:
    void SentryWorker();
    void GetBMCIp();
    void ReportAlarm(const IPMIEvent& event);
    void ReportResult(int resultLevel, const std::string& msg);
    int QueryEvents();
    std::string BuildIPMICommand(uint16_t startIndex);
    std::vector<std::string> ExecuteIPMICommand(const std::string& cmd);
    ResponseHeader ParseResponseHeader(const std::vector<std::string>& hexBytes);
    IPMIEvent ParseSingleEvent(const std::vector<std::string>& hexBytes, size_t startPos);
    void ProcessEvents(const std::vector<std::string>& hexBytes, uint8_t eventCount);

private:
    std::atomic<bool> m_running;
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::string m_bmcIp;
    std::set<uint8_t> m_lastDeviceIds;
    std::set<uint8_t> m_currentDeviceIds;
    int m_patrolSeconds;
    int m_alarmId;
};
}
#endif

