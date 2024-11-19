#ifndef __LOGGER_H
#define __LOGGER_H

#define TOOL_NAME "hbm_online_repair"

enum log_level {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
};

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
