#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <traceevent/kbuffer.h>
#include "ras-non-standard-handler.h"
#include "logger.h"

static int uuid_le(const char *uu, char* uuid)
{
    if (!uu) {
        log(LOG_ERROR, "uuid_le failed: uu is empty");
        return -1;
    }
    size_t uu_len = strlen(uu);
    if (uu_len != SECTION_TYPE_UUID_LEN) {
        log(LOG_ERROR, "uuid_le failed: uu len is incorrect");
        return -1;
    }
    size_t uuid_len = strlen(uuid);
    if (uuid_len != strlen(UUID_STR_TYPE)) {
        log(LOG_ERROR, "uuid_le failed: uuid len is incorrect");
        return -1;
    }

    char *p = uuid;
    int i;
    static const unsigned char le[16] = {3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};

    for (i = 0; i < 16; i++) {
        p += sprintf(p, "%.2x", (unsigned char) uu[le[i]]);
        switch (i) {
        case 3:
        case 5:
        case 7:
        case 9:
            *p++ = '-';
            break;
        }
    }

    *p = 0;

    return 0;
}

int ras_non_standard_event_handler(struct trace_seq *s,
             struct tep_record *record,
             struct tep_event *event, void *context)
{
    int len;
    unsigned long long val;
    struct ras_non_standard_event ev;

    ev.sec_type = tep_get_field_raw(s, event, "sec_type",
                       record, &len, 1);
    if(!ev.sec_type) {
        log(LOG_WARNING, "get event section type failed\n");
        return -1;
    }

    trace_seq_printf(s, "\n");
    char uuid[sizeof(UUID_STR_TYPE)] = UUID_STR_TYPE;
    if (uuid_le(ev.sec_type, uuid) < 0) {
        log(LOG_WARNING, "get uuid failed\n");
        return -1;
    }
    trace_seq_printf(s, "sec_type: %s\n", uuid);

    if (tep_get_field_val(s, event, "len", record, &val, 1) < 0) {
        log(LOG_WARNING, "tep get field val failed\n");
        return -1;
    }

    ev.length = val;
    trace_seq_printf(s, "length: %d\n", ev.length);

    ev.error = tep_get_field_raw(s, event, "buf", record, &len, 1);
    if(!ev.error || ev.length != len) {
        log(LOG_WARNING, "get event error failed\n");
        return -1;
    }

    if (strcmp(uuid, HISI_COMMON_SECTION_TYPE_UUID) == 0) {
        decode_hisi_common_section(&ev);
    }

    return 0;
}
