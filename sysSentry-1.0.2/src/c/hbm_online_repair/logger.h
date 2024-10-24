#ifndef __LOGGER_H
#define __LOGGER_H

#define TOOL_NAME "hbm_online_repair"

#define LOG_DEBUG   0
#define LOG_INFO    1
#define LOG_WARNING 2
#define LOG_ERROR   3

extern int global_level_setting;

#define log_prefix(level) \
	(level == LOG_DEBUG ? "DEBUG" : \
	 level == LOG_INFO ? "INFO" : \
	 level == LOG_WARNING ? "WARNING" : \
	 level == LOG_ERROR ? "ERROR" : \
	 "UNKNOWN_LEVEL")

#define log_fd(level) \
	(level == LOG_ERROR ? stderr : stdout)

#define log(level, fmt, args...) do {\
	if (level >= global_level_setting) {\
		fprintf(log_fd(level), "[%s] %s: ", log_prefix(level), TOOL_NAME);\
		fprintf(log_fd(level), fmt, ##args);\
		fflush(log_fd(level));\
	}\
} while (0)

#endif
