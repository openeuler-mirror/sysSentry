#ifndef __RAS_NON_STANDARD_HANDLER_H
#define __RAS_NON_STANDARD_HANDLER_H

#include <traceevent/event-parse.h>
#include "ras-events.h"

#define BIT(nr) (1UL << (nr))

#define SECTION_TYPE_UUID_LEN         16
#define UUID_STR_TYPE                 "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define HISI_COMMON_SECTION_TYPE_UUID "c8b328a8-9917-4af6-9a13-2e08ab2e7586"

struct ras_non_standard_event {
    char timestamp[64];
    const char *sec_type;
    const uint8_t *error;
    uint32_t length;
};

int ras_non_standard_event_handler(struct trace_seq *s,
            struct tep_record *record,
            struct tep_event *event, void *context);

int decode_hisi_common_section(struct ras_non_standard_event *event);

#endif
