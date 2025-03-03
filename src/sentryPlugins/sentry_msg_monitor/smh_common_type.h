#ifndef SMH_COMMON_TYPE_H
#define SMH_COMMON_TYPE_H

#include <stdint.h>

#define SMH_TYPE ('}')
#define MAX_NUMA_NODES 8

enum {
	SMH_CMD_MSG_ACK = 0x10,
};

#define SMH_MSG_ACK _IO(SMH_TYPE, SMH_CMD_MSG_ACK)

enum sentry_msg_helper_msg_type {
	SMH_MESSAGE_POWER_OFF,
	SMH_MESSAGE_OOM,
	SMH_MESSAGE_MAX,
};

struct sentry_msg_helper_msg {
	enum sentry_msg_helper_msg_type type;
	uint64_t msgid;
	// reboot_info is empty
	struct {
		int nr_nid;
		int nid[MAX_NUMA_NODES];
		int sync;
		int timeout;
		int reason;
	} oom_info;
	unsigned long res;
};

#endif
