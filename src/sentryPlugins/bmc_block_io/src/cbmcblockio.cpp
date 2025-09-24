/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * bmc_block_io is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * Author: hewanhan@h-partners.com
 */

#include "cbmcblockio.h"
#include <string>
#include <chrono>
#include <cstdint>
#include <json-c/json.h>
#include <sstream>
#include <algorithm>
#include <map>
extern "C" {
#include "register_xalarm.h"
}
#include "common.h"
#include "configure.h"
#include "logger.h"

namespace BMCBlockIoPlu {

const int RESP_HEADER_SIZE = 7;
const int EVENT_SIZE = 15;
const uint32_t ALARM_OCCUR_CODE = 0x02000039;
const uint32_t ALARM_CLEAR_CODE = 0x0200003A;
const std::string BMC_TASK_NAME = "bmc_block_io";
const std::string GET_BMCIP_CMD = "ipmitool lan print";
const std::string IPMI_KEY_IP_ADDR = "IP Address";
const std::string MSG_BMCIP_EMPTY = "ipmitool get bmc ip failed.";
const std::string MSG_BMC_QUERY_FAIL = "ipmitool query failed.";
const std::string MSG_EXIT_SUCCESS = "receive exit signal, task completed.";
const std::string JSON_KEY_MSG = "msg";
const std::string JSON_KEY_ALARM_SOURCE = "alarm_source";
const std::string JSON_KEY_DRIVER_NAME = "driver_name";
const std::string JSON_KEY_IO_TYPE = "io_type";
const std::string JSON_KEY_REASON = "reason";
const std::string JSON_KEY_BLOCK_STACK = "block_stack";
const std::string JSON_KEY_DETAILS = "details";
const std::string MOD_SECTION_COMMON = "common";
const std::string MOD_COMMON_ALARM_ID = "alarm_id";

CBMCBlockIo::CBMCBlockIo() :
    m_running(false),
    m_patrolSeconds(BMCPLU_PATROL_DEFAULT)
{
    std::map<std::string, std::map<std::string, std::string>> modConfig = parseModConfig(BMCPLU_MOD_CONFIG);
    if (modConfig.find(MOD_SECTION_COMMON) != modConfig.end()) {
        auto commonSection = modConfig[MOD_SECTION_COMMON];
        if (commonSection.find(MOD_COMMON_ALARM_ID) != commonSection.end()) {
            int alarmId = 0;
            if (IsValidNumber(commonSection[MOD_COMMON_ALARM_ID], alarmId) && alarmId > 0) {
                m_alarmId = alarmId;
            } else {
                m_alarmId = BMCPLU_DEFAULT_ALARM_ID;
                BMC_LOG_WARNING << "Invalid alarm_id in mod config, use default alarm id: " << BMCPLU_DEFAULT_ALARM_ID;
            }
        }
    }
}

CBMCBlockIo::~CBMCBlockIo()
{
}

void CBMCBlockIo::Start()
{
    if (m_running) {
        return;
    }

    GetBMCIp();
    if (m_bmcIp.empty()) {
        BMC_LOG_ERROR << "BMC Ip is empty.";
        ReportResult(RESULT_LEVEL_FAIL, MSG_BMCIP_EMPTY);
        return;
    }
    m_running = true;
    m_worker = std::thread(&CBMCBlockIo::SentryWorker, this);
    BMC_LOG_INFO << "BMC block io Start.";
}

void CBMCBlockIo::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_cv.notify_all();

    if (m_worker.joinable()) {
        m_worker.join();
    }
    BMC_LOG_INFO <<"BMC block io Stop.";
}

void CBMCBlockIo::SetPatrolInterval(int seconds)
{
    m_patrolSeconds = seconds;
}

bool CBMCBlockIo::IsRunning()
{
    return m_running;
}

void CBMCBlockIo::SentryWorker()
{
    int ret = BMCPLU_SUCCESS;
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::seconds(m_patrolSeconds), [this] {
            return !m_running;
        });

        if (!m_running) {
            break;
        }
        ret = QueryEvents();
        if (ret != BMCPLU_SUCCESS) {
            break;
        }
    }

    if (ret == BMCPLU_SUCCESS) {
        ReportResult(RESULT_LEVEL_PASS, MSG_EXIT_SUCCESS);
    } else {
        ReportResult(RESULT_LEVEL_FAIL, MSG_BMC_QUERY_FAIL);
    }
    m_running = false;
    BMC_LOG_INFO << "BMC block io SentryWorker exit.";
    return;
}

void CBMCBlockIo::GetBMCIp()
{
    std::vector<std::string> result;
    if (ExecCommand(GET_BMCIP_CMD, result)) {
        return;
    }
    for (const auto& iter: result) {
        if (iter.find(IPMI_KEY_IP_ADDR) != std::string::npos) {
            size_t eq_pos = iter.find(':');
            if (eq_pos != std::string::npos) {
                std::string key = Trim(iter.substr(0, eq_pos));
                std::string value = Trim(iter.substr(eq_pos + 1));
                if (key == IPMI_KEY_IP_ADDR) {
                    m_bmcIp = value;
                    return;
                }
            }
        }
    }
    return;
}

/***** ipml protocol *****/
/*
请求 字节顺序 含义
     1-3     厂商id 默认0xDB 0x07 0x0
     4       子命令 默认0x40
     5       请求类型 默认0x00
     6-7     需要查询的事件起始编号,某些情况下查询到的事件可能有多条,
             单次响应无法全部返回,因此需要修改该值分页查询
     8       事件严重级别 位图形式,bit0-normal,bit1-minor,bit2-major,bit3-critical,慢盘事件只支持normal
     9       主体类型 硬盘类型0x02
响应 字节顺序 含义
     1       completion code 调用成功时该字节不会显示在终端上
     2-4     厂商ID,对应请求中内容
     5-6     事件总数量
     7       本次返回中包含的事件数量
     8       占位字节,默认0
     9-12    告警类型码,0x0200039为告警产生,0x0200003A为告警消除
     13-16   事件发生的linux时间戳
     17      事件严重级别,0-normal,1-minor,2-major,3-critical
     18      主体类型,对应请求中内容
     19      设备序号 带外编号
     20-23   占位字节,默认0
     N+1-N+15重复上面9-23中的内容,表示下一个事件
厂商ID固定,其他所有多字节对象均为小端序, eg:
ipmitool raw 0x30 0x94 0xDB 0x07 0x00 0x40 0x00 0x00 0x00 0x01 0x02
db 07 00 03 00 03 00 39 00 00 02 2f ab 91 68 00 02 04 00 00 00 00
39 00 00 02 2e ab 91 68 00 02 02 00 00 00 00 39 00 00 02 2e ab 91
68 00 02 01 00 00 00 00
    */
int CBMCBlockIo::QueryEvents()
{
    uint16_t currentIndex = 0;
    int ret = BMCPLU_SUCCESS;
    m_currentDeviceIds.clear();

    while (true) {
        std::string cmd = BuildIPMICommand(currentIndex);
        std::vector<std::string> hexBytes = ExecuteIPMICommand(cmd);
        if (hexBytes.empty()) {
            break;
        }

        ResponseHeader header = ParseResponseHeader(hexBytes);
        if (!header.valid) {
            ret = BMCPLU_FAILED;
            break;
        }

        BMC_LOG_INFO << "Total events: " << header.totalEvents
                     << ", returned: " << static_cast<int>(header.eventCount)
                     << ", current index: " << currentIndex;
        if (header.eventCount == 0) {
            break;
        }

        size_t expectedSize = RESP_HEADER_SIZE + header.eventCount * EVENT_SIZE;
        if (hexBytes.size() != expectedSize) {
            BMC_LOG_ERROR << "Response size invalid. Expected: " << expectedSize
                         << ", Actual: " << hexBytes.size();
            ret = BMCPLU_FAILED;
            break;
        }

        ProcessEvents(hexBytes, header.eventCount);
        currentIndex += header.eventCount;

        if (currentIndex >= header.totalEvents) {
            break;
        }
    }

    if (ret == BMCPLU_SUCCESS) {
        for (const auto& id : m_lastDeviceIds) {
            if (m_currentDeviceIds.find(id) == m_currentDeviceIds.end()) {
                uint32_t timeNow = static_cast<uint32_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
                IPMIEvent clearEvent = {ALARM_CLEAR_CODE, timeNow, 0, 0x02, id, true};
                ReportAlarm(clearEvent);
            }
        }
        m_lastDeviceIds = m_currentDeviceIds;
    }
    return ret;
}

std::string CBMCBlockIo::BuildIPMICommand(uint16_t startIndex)
{
    uint8_t indexHigh = static_cast<uint8_t>((startIndex >> 8) & 0xff);
    uint8_t indexLow = static_cast<uint8_t>(startIndex & 0xff);
    std::ostringstream cmdStream;
    cmdStream << "ipmitool raw 0x30 0x94 0xDB 0x07 0x00 0x40 0x00"
            << " " << ByteToHex(indexLow)
            << " " << ByteToHex(indexHigh)
            << " 0x01 0x02";
    return cmdStream.str();
}

std::vector<std::string> CBMCBlockIo::ExecuteIPMICommand(const std::string& cmd)
{
    BMC_LOG_DEBUG << "IPMI event query command: " << cmd;

    std::vector<std::string> cmdOut;
    if (ExecCommand(cmd, cmdOut)) {
        BMC_LOG_ERROR << "IPMI command execute failed.";
        return {};
    }

    std::ostringstream responseStream;
    for (size_t i = 0; i < cmdOut.size(); ++i) {
        std::string line = cmdOut[i];
        BMC_LOG_DEBUG << "Execute IPMI event response: " << line;
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        if (i > 0 && !line.empty()) {
            responseStream << ' ';
        }
        responseStream << line;
    }
    return SplitString(responseStream.str(), " ");
}

ResponseHeader CBMCBlockIo::ParseResponseHeader(const std::vector<std::string>& hexBytes)
{
    ResponseHeader header = {0, 0, false};

    if (hexBytes.size() < RESP_HEADER_SIZE) {
        BMC_LOG_ERROR << "Invalid response length: " << hexBytes.size();
        return header;
    }

    if (hexBytes[0] != "db" || hexBytes[1] != "07" || hexBytes[2] != "00") {
        BMC_LOG_ERROR << "Unexpected manufacturer ID: " 
                     << hexBytes[0] << " " << hexBytes[1] << " " << hexBytes[2];
        return header;
    }

    char* endPtr = nullptr;
    unsigned long totalLow = std::strtoul(hexBytes[3].c_str(), &endPtr, 16);
    if (endPtr == hexBytes[3].c_str() || *endPtr != '\0' || totalLow > 0xff) {
        BMC_LOG_ERROR << "Invalid totalLow byte: " << hexBytes[3];
        return header;
    }

    unsigned long totalHigh = std::strtoul(hexBytes[4].c_str(), &endPtr, 16);
    if (endPtr == hexBytes[4].c_str() || *endPtr != '\0' || totalHigh > 0xff) {
        BMC_LOG_ERROR << "Invalid totalHigh byte: " << hexBytes[4];
        return header;
    }

    header.totalEvents = static_cast<uint16_t>(totalLow) | (static_cast<uint16_t>(totalHigh) << 8);
    unsigned long count = std::strtoul(hexBytes[5].c_str(), &endPtr, 16);
    if (endPtr == hexBytes[5].c_str() || *endPtr != '\0' || count > 0xff) {
        BMC_LOG_ERROR << "Invalid event count byte: " << hexBytes[5];
        return header;
    }

    header.eventCount = static_cast<uint8_t>(count);
    header.valid = true;
    return header;
}

IPMIEvent CBMCBlockIo::ParseSingleEvent(const std::vector<std::string>& hexBytes, size_t startPos)
{
    IPMIEvent event = {0, 0, 0, 0, 0, false};
    char* endPtr = nullptr;

    for (int i = 0; i < 4; ++i) { 
        unsigned long byte = std::strtoul(hexBytes[startPos + i].c_str(), &endPtr, 16);
        if (endPtr == hexBytes[startPos + i].c_str() || *endPtr != '\0' || byte > 0xff) {
            BMC_LOG_ERROR << "Invalid alarm type byte at pos " << startPos + i 
                         << ": " << hexBytes[startPos + i];
            return event;
        }
        event.alarmTypeCode |= (static_cast<uint32_t>(byte) << (i * 8));
    }

    for (int i = 0; i < 4; ++i) {
        unsigned long byte = std::strtoul(hexBytes[startPos + 4 + i].c_str(), &endPtr, 16);
        if (endPtr == hexBytes[startPos + 4 + i].c_str() || *endPtr != '\0' || byte > 0xff) {
            BMC_LOG_ERROR << "Invalid timestamp byte at pos " << startPos + 4 + i 
                         << ": " << hexBytes[startPos + 4 + i];
            return event;
        }
        event.timestamp |= (static_cast<uint32_t>(byte) << (i * 8));
    }

    unsigned long severity = std::strtoul(hexBytes[startPos + 8].c_str(), &endPtr, 16);
    if (endPtr == hexBytes[startPos + 8].c_str() || *endPtr != '\0' || severity > 0xff) {
        BMC_LOG_ERROR << "Invalid severity byte: " << hexBytes[startPos + 8];
        return event;
    }
    event.severity = static_cast<uint8_t>(severity);

    unsigned long subjectType = std::strtoul(hexBytes[startPos + 9].c_str(), &endPtr, 16);
    if (endPtr == hexBytes[startPos + 9].c_str() || *endPtr != '\0' || subjectType > 0xff) {
        BMC_LOG_ERROR << "Invalid subject type byte: " << hexBytes[startPos + 9];
        return event;
    }
    event.subjectType = static_cast<uint8_t>(subjectType);

    unsigned long deviceId = std::strtoul(hexBytes[startPos + 10].c_str(), &endPtr, 16);
    if (endPtr == hexBytes[startPos + 10].c_str() || *endPtr != '\0' || deviceId > 0xff) {
        BMC_LOG_ERROR << "Invalid device ID byte: " << hexBytes[startPos + 10];
        return event;
    }
    event.deviceId = static_cast<uint8_t>(deviceId);

    event.valid = true;
    return event;
}

void CBMCBlockIo::ProcessEvents(const std::vector<std::string>& hexBytes, uint8_t eventCount)
{
    for (int i = 0; i < eventCount; ++i) {
        size_t startPos = RESP_HEADER_SIZE + i * EVENT_SIZE;

        IPMIEvent event = ParseSingleEvent(hexBytes, startPos);
        if (!event.valid) {
            continue;
        }

        ReportAlarm(event);
    }
    return;
}

void CBMCBlockIo::ReportAlarm(const IPMIEvent& event)
{
    uint8_t ucAlarmLevel = MINOR_ALM;
    uint8_t ucAlarmType = 0;
    if (event.alarmTypeCode == ALARM_OCCUR_CODE) {
        ucAlarmType = ALARM_TYPE_OCCUR;
        m_currentDeviceIds.insert(event.deviceId);
    } else if (event.alarmTypeCode == ALARM_CLEAR_CODE) {
        ucAlarmType = ALARM_TYPE_RECOVER;
    } else {
        BMC_LOG_DEBUG << "Skipping unknown alarm type: 0x"
                     << std::hex << event.alarmTypeCode;
        return;
    }

    BMC_LOG_INFO << "Report alarm, type: " << static_cast<int>(ucAlarmType);
    BMC_LOG_INFO << "level: " << static_cast<int>(ucAlarmLevel);
    BMC_LOG_INFO << "deviceId: " << static_cast<int>(event.deviceId);
    BMC_LOG_INFO << "timestamp: " << event.timestamp;
    json_object* jObject = json_object_new_object();
    json_object_object_add(jObject, JSON_KEY_ALARM_SOURCE.c_str(), json_object_new_string(BMC_TASK_NAME.c_str()));
    json_object_object_add(jObject, JSON_KEY_DRIVER_NAME.c_str(), json_object_new_string(std::to_string(event.deviceId).c_str()));
    json_object_object_add(jObject, JSON_KEY_IO_TYPE.c_str(), json_object_new_string("read,write"));
    json_object_object_add(jObject, JSON_KEY_REASON.c_str(), json_object_new_string("driver slow"));
    json_object_object_add(jObject, JSON_KEY_BLOCK_STACK.c_str(), json_object_new_string("rq_driver"));
    json_object_object_add(jObject, JSON_KEY_DETAILS.c_str(), json_object_new_string("{}}"));
    const char *jData = json_object_to_json_string(jObject);
    int ret = xalarm_Report(m_alarmId, ucAlarmLevel, ucAlarmType, const_cast<char*>(jData));
    if (ret != RETURE_CODE_SUCCESS) {
        BMC_LOG_ERROR << "Failed to xalarm_Report, ret: " << ret;
    }
    json_object_put(jObject);
    return;
}

void CBMCBlockIo::ReportResult(int resultLevel, const std::string& msg)
{
    RESULT_LEVEL level = static_cast<RESULT_LEVEL>(resultLevel);
    json_object* jObject = json_object_new_object();
    json_object_object_add(jObject, JSON_KEY_MSG.c_str(), json_object_new_string(msg.c_str()));
    const char *jData = json_object_to_json_string(jObject);
    int ret = report_result(BMC_TASK_NAME.c_str(), level, const_cast<char*>(jData));
    if (ret != RETURE_CODE_SUCCESS) {
        BMC_LOG_ERROR << "Failed to report_result, ret: " << ret;
    }
    json_object_put(jObject);
    return;
}
};
