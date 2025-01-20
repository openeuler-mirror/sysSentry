#ifndef __RAS_EVENTS_H
#define __RAS_EVENTS_H

#include <stdint.h>
#include <time.h>

#define MAX_PATH 1024

#define DEFAULT_PAGE_SIZE  4096

struct ras_events {
    char              tracing[MAX_PATH + 1];
    struct tep_handle *pevent;
    int               page_size;
};

struct pcpu_data {
    struct tep_handle *pevent;
    struct ras_events *ras;
    int               cpu;
};

/* Function prototypes */
int toggle_ras_event(char *trace_dir, char *group, char *event, int enable);
int handle_ras_events(struct ras_events *ras);
struct ras_events *init_trace_instance(void);

#endif
