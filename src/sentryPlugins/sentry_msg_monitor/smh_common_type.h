#ifndef SMH_COMMON_TYPE_H
#define SMH_COMMON_TYPE_H

#include <stdint.h>

#define SMH_TYPE ('}')

enum {
	SMH_CMD_MSG_ACK = 0x10,
};

#define SMH_MSG_ACK _IO(SMH_TYPE, SMH_CMD_MSG_ACK)

enum sentry_msg_helper_msg_type {
	SMH_MESSAGE_POWER_OFF,
	SMH_MESSAGE_MAX,
};

struct sentry_msg_helper_msg {
	enum sentry_msg_helper_msg_type type;
	uint64_t msgid;
	int res;
};

#endif
