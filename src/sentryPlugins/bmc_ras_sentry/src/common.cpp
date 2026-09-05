/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * bmc_ras_sentry is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * Author: hewanhan@h-partners.com
 */

#include "common.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <sys/wait.h>
#include <unordered_map>
#include <string>
#include <regex>

namespace BMCRasSentryPlu {

std::string Trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\n\r\v\f");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r\v\f");
    return str.substr(first, (last - first + 1));
}

bool IsValidNumber(const std::string& str, int& num)
{
    if (str.empty()) {
        return false;
    }
    for (const auto& iter : str) {
        if (!std::isdigit(iter)) {
            return false;
        }
    }
    std::istringstream iss(str);
    if (!(iss >> num)) {
        return false;
    }
    return true;
}

int ParseConfig(const std::string& path, PluConfig& config)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        BMC_LOG_ERROR << "Failed to open config file: " << path;
        return BMCPLU_FAILED;
    }

    std::unordered_map<std::string, ConfigItem> configMap;
    configMap["log_level"] = {true, false, [&](const std::string& value) {
        if (value == "debug") {
            config.logLevel = Logger::Level::Debug;
        } else if (value == "info") {
            config.logLevel = Logger::Level::Info;
        } else if (value == "warning") {
            config.logLevel = Logger::Level::Warning;
        } else if (value == "error") {
            config.logLevel = Logger::Level::Error;
        } else if (value == "critical") {
            config.logLevel = Logger::Level::Critical;
        } else {
            BMC_LOG_ERROR << "Invalid log_level value.";
            return false;
        }
        return true;
    }};

    configMap["patrol_second"] = {true, false, [&](const std::string& value) {
        int num = 0;
        if (!IsValidNumber(value, num) || !(num >= BMCPLU_PATROL_MIN && num <= BMCPLU_PATROL_MAX)) {
            BMC_LOG_ERROR << "Invalid patrol_second value.";
            return false;
        }
        config.patrolSeconds = num;
        return true;
    }};

    configMap["bmc_events"] = {true, false, [&](const std::string& value) {
        const std::regex event_id_regex("^\\d{4}$");
        auto result = SplitString(value, ",");

        for (const auto& event_id : result) {
            if (!std::regex_match(event_id, event_id_regex)) {
                BMC_LOG_ERROR << "BMC Events parse error, value: " << value << ", event id: " << event_id;
                return false;
            }
        }
        config.BMCEvents = result;
        return true;
    }};

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos || eqPos == 0) {
            BMC_LOG_ERROR << "Config file format invalid.";
            return BMCPLU_FAILED;
        }

        std::string key = Trim(line.substr(0, eqPos));
        std::string value = Trim(line.substr(eqPos + 1));
        if (value.empty()) {
            BMC_LOG_ERROR << "Config key: " << key << " cannot empty.";
            return BMCPLU_FAILED;
        }

        auto iter = configMap.find(key);
        if (iter == configMap.end()) {
            BMC_LOG_ERROR << "Config error, unknown key : " << key;
            return BMCPLU_FAILED;
        }

        if (!iter->second.processor(value)) {
            return BMCPLU_FAILED;
        }
        iter->second.found = true;
    }

    for (const auto& iter : configMap) {
        if (iter.second.required && !iter.second.found) {
            BMC_LOG_ERROR << "Config error, missing required key : " << iter.first;
            return BMCPLU_FAILED;
        }
    }
    return BMCPLU_SUCCESS;
}

std::map<std::string, std::map<std::string, std::string>> parseModConfig(const std::string& path)
{
    std::map<std::string, std::map<std::string, std::string>> result;

    std::ifstream file(path);
    if (!file.is_open()) {
        BMC_LOG_ERROR << "Failed to open mod file: " << path;
        return result;
    }

    std::string line;
    std::string currentSection;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // check for section
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            currentSection = Trim(line.substr(1, line.length() - 2));
            if (!currentSection.empty()) {
                result[currentSection] = std::map<std::string, std::string>();
            }
            continue;
        }

        // check for key=value
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos && !currentSection.empty()) {
            std::string key = Trim(line.substr(0, eqPos));
            std::string value = Trim(line.substr(eqPos + 1));
            if (!key.empty()) {
                result[currentSection][key] = value;
            }
        }
    }

    return result;
}

std::string ExtractFileName(const std::string& path)
{
    size_t lastSlashPos = path.find_last_of('/');
    if (lastSlashPos == std::string::npos) {
        return path;
    } else {
        return path.substr(lastSlashPos + 1);
    }
}

int ExecCommand(const std::string& cmd, std::vector<std::string>& result)
{
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        BMC_LOG_ERROR << "Cmd: " << cmd << ", popen failed.";
        return BMCPLU_FAILED;
    }

    char buffer[512];
    result.clear();
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result.push_back(Trim(buffer));
    }

    int status = pclose(pipe);
    if (status == -1) {
        BMC_LOG_ERROR << "Cmd: " << cmd << ", pclose failed.";
        return BMCPLU_FAILED;
    } else {
        int exitCode = WEXITSTATUS(status);
        if (exitCode != 0) {
            BMC_LOG_ERROR << "Cmd: " << cmd << ", exit failed, err code: " << exitCode;
            return BMCPLU_FAILED;
        }
    }
    return BMCPLU_SUCCESS;
}

std::string ByteToHex(uint8_t byte)
{
    std::ostringstream oss;
    const int hexLen = 2;
    oss << std::hex << std::setfill('0') << std::setw(hexLen) << static_cast<int>(byte);
    return "0x" + oss.str();
}

bool HexAsciiToChar(const std::string& hexStr, std::string& asciiStr)
{
    if (hexStr.length() != 2) {
        BMC_LOG_ERROR << "input str length for hex ascii to char must be 2, str: " << hexStr;
        return false;
    }

    // endPtr完整性检查: 确认整个字符串被完整消费, 防御strtoul的宽松解析
    // (前导空白/正负号/部分解析取前缀值)在前置白名单被改动后静默失效
    uint8_t asciiVal = 0;
    char* endPtr = nullptr;
    unsigned long temp = std::strtoul(hexStr.c_str(), &endPtr, 16);
    if (endPtr != hexStr.c_str() + hexStr.length()) {
        BMC_LOG_ERROR << "input str is not fully consumed as hex, str: " << hexStr;
        return false;
    }
    if (temp > UINT8_MAX) {
        BMC_LOG_ERROR << "input str value out of 255, str: " << hexStr;
        return false;
    }
    asciiVal = static_cast<uint8_t>(temp);
    asciiStr = static_cast<char>(asciiVal);

    return true;
}

std::vector<std::string> SplitString(const std::string& str, const std::string& split)
{
    std::vector<std::string> result;
    if (split.empty()) {
        result.push_back(str);
        return result;
    }

    size_t pos = 0;
    while (true) {
        size_t splitPos = str.find(split, pos);
        std::string subString = str.substr(pos, splitPos - pos);
        if (!subString.empty()) {
            result.push_back(subString);
        }

        if (splitPos == std::string::npos) {
            break;
        }
        pos = splitPos + split.size();
    }
    return result;
}

std::vector<std::string> SplitBySpace(const std::string& str)
{
    std::vector<std::string> result;
    std::regex reg("\\s+");
    std::sregex_token_iterator it(str.begin(), str.end(), reg, -1);
    std::sregex_token_iterator end;
    for (; it != end; ++it) {
        std::string token = Trim(*it);
        if (!token.empty()) {
            result.push_back(token);
        }
    }

    return result;
}

/**
 * IsValidSafeToken - 校验外部工具输出中解析出的字符串是否只含安全字符
 *
 * 使用场景(为什么需要本函数):
 *   本插件以root权限运行, 会解析raid卡管理工具(storcli64/hiraidadm)的输出,
 *   从中提取 ctrlId/VDId/encId/slotId/VDName 等字段, 再经 format_string 拼接成
 *   新的命令字符串, 最终由 ExecCommand() -> popen() 以shell方式执行。
 *   若上述工具的输出被恶意构造(如raid卡固件被劫持、盘柜背板数据被篡改,
 *   或工具二进制被替换), 字段中携带 "; rm -rf /" 之类的shell元字符即可
 *   注入任意命令。因此所有取自外部工具输出、将要拼入命令的字符串,
 *   必须先通过本函数校验。
 *
 * 校验规则(白名单):
 *   - 字母、数字          : 编号、盘名、序列号的主体字符
 *   - ':'                 : storcli EID:Slt 字段分隔符, 如 "259:0"
 *   - '/'                 : 设备路径与 DG/VD 字段分隔符, 如 "/dev/sda"、"0/0"
 *   - '-'                 : 设备名连字符, 如 "nvme0n1-e1"; 也是 "N/A" 的组成部分
 *   - '_'                 : 设备名下划线, 如部分厂商的RAID VD名, 无shell语义
 *   - 空串                : 合法。工具输出中字段可能为空(如直通盘无VD名称),
 *                          空值不会注入任何内容, 拼入命令后只是一个空参数位置
 *   - "NA" / "N/A"        : 合法。raid工具对无值字段的惯用占位符(不适用/未检出),
 *                          仅含字母与斜杠, 无注入风险
 *   - 其余任何字符均拒绝  : 包括但不限于 ; | & ` $ ( ) < > " ' \ 空白符和控制字符,
 *                          这些是shell语法字符或可破坏命令语义的字符
 *
 * 使用约定:
 *   - 新增解析外部输出的字段时, 若该字段会拼入命令, 必须先经本函数校验;
 *     仅进入JSON上报的字段也建议校验, 防止告警内容注入
 *   - 需要放行新字符时, 先确认该字符无shell语义(参考POSIX shell语法中的
 *     特殊字符表), 且当前 storcli/hiraidadm 版本的真实输出确实包含该字符,
 *     再添加到白名单
 *
 * 注意: 本函数是纵深防御的一层, 根治方案是 ExecCommand 改用 execvp 直接
 *   执行(argv数组, 不经shell解析), 若后续改造请同步评估本函数的保留价值
 */
bool IsValidSafeToken(const std::string& str)
{
    // 空串与raid工具的惯用占位符均为合法输入
    if (str.empty() || str == "NA" || str == "N/A") {
        return true;
    }

    for (const auto& ch : str) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            continue;
        }
        if (ch == ':' || ch == '/' || ch == '-' || ch == '_') {
            continue;
        }
        return false;
    }

    return true;
}

std::map<std::string, std::vector<std::string> > ParseStorcliCmd(const std::string& cmd)
{
    std::vector<std::string> cmdOut;
    if (ExecCommand(cmd, cmdOut)) {
        return {};
    }

    std::map<std::string, std::vector<std::string> > result;
    size_t startLine = 0, endLine = 0;
    for (size_t i = 0; i < cmdOut.size(); i++) {
        if ((cmdOut[i].size() == 0 || cmdOut[i] != std::string(cmdOut[i].size(), '=')) && i != cmdOut.size() - 1)
            continue;

        std::string strKey;
        if (i == cmdOut.size() - 1) {
            endLine = i;
        } else {
            endLine = i - 2;
        }

        if (endLine > cmdOut.size() || startLine > endLine) {
            BMC_LOG_ERROR << "parse storcli message failed, cmd:" << cmd;
            return {};
        }

        std::vector<std::string> storcliInfo(cmdOut.begin() + startLine, cmdOut.begin() + endLine + 1);
        if (startLine == 0) {
            strKey = "head message";
        } else {
            strKey = cmdOut[startLine - 2];
        }
        result.emplace(strKey, storcliInfo);
        startLine = i + 1;
    }

    return result;
}

std::pair<std::map<std::string, uint8_t>, std::vector<std::vector<std::string> > > ParseCmdMap(
    const std::vector<std::string>& inputVec)
{
    int i = 0;
    std::map<std::string, uint8_t> mapHead;
    std::vector<std::vector<std::string> > mapInfo;
    for (const auto& line : inputVec) {
        if (line.size() != 0 && line == std::string(line.size(), '-')) {
            ++i;
            if (i == 3)
                break;
            continue;
        }

        if (i == 1) {
            auto head = SplitBySpace(line);
            // 列数远超真实表头(几十列以内)说明输出异常(不可信工具输出被构造),
            // 列索引以uint8_t存储, 上限需与之匹配; 拒绝而非继续解析, 防止越界/截断
            if (head.size() > UINT8_MAX) {
                BMC_LOG_ERROR << "cmd map head columns exceeded, column count: " << head.size();
                return {};
            }
            for (uint8_t j = 0; j < head.size(); j++) {
                mapHead.emplace(head[j], j);
            }
        } else if (i == 2) {
            auto value = SplitBySpace(line);
            mapInfo.push_back(value);
        }
    }

    return {mapHead, mapInfo};
}

std::map<std::string, std::string> ParseStorcliKeyToValue(const std::vector<std::string>& inputVec)
{
    std::map<std::string, std::string> result;
    for (const auto& line : inputVec) {
        size_t equal_pos = line.find('=');
        if (equal_pos == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, equal_pos));
        std::string value = Trim(line.substr(equal_pos + 1));

        result[key] = value;
    }

    return result;
}

json_object* ParseHiraidadmCmd(const std::string& cmd)
{
    std::vector<std::string> cmdOut;
    if (ExecCommand(cmd, cmdOut)) {
        return nullptr;
    }

    std::string jsonStr;
    for (const auto& line : cmdOut) {
        jsonStr += line;
    }

    auto rootObj = json_tokener_parse(jsonStr.c_str());
    if (rootObj == NULL) {
        BMC_LOG_WARNING << "parse json value failed, cmd: " << cmd;
        return nullptr;
    }

    auto dataObj = json_object_object_get(rootObj, "CommandData");
    if (!json_object_is_type(dataObj, json_type_object)) {
        BMC_LOG_WARNING << "CommandData object can't be find, cmd: " << cmd;
        json_object_put(rootObj);
        return nullptr;
    }

    json_object_get(dataObj);
    json_object_put(rootObj);

    return dataObj;
}

std::string Uint32ToHexString(uint32_t num)
{
    std::ostringstream oss;
    int length = 8;
    oss << std::uppercase;
    oss << "0x";
    oss << std::setw(length) << std::setfill('0') << std::hex << num;
    return oss.str();
}

std::string Uint32ToLocalTime(uint32_t timestamp)
{
    time_t t = static_cast<time_t>(timestamp);

    struct tm* localTm = localtime(&t);
    if (localTm == nullptr) {
        BMC_LOG_WARNING << "parse timestamp error, value:" << timestamp;
        return "";
    }

    char timeBuf[64] = {0};
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", localTm);

    return std::string(timeBuf);
}
}
