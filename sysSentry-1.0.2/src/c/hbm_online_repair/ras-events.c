#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <traceevent/kbuffer.h>
#include <traceevent/event-parse.h>
#include "ras-non-standard-handler.h"
#include "logger.h"

/*
 * Polling time, if read() doesn't block. Currently, trace_pipe_raw never
 * blocks on read(). So, we need to sleep for a while, to avoid spending
 * too much CPU cycles. A fix for it is expected for 3.10.
 */
#define POLLING_TIME 3

/* Test for a little-endian machine */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define ENDIAN KBUFFER_ENDIAN_LITTLE
#else
    #define ENDIAN KBUFFER_ENDIAN_BIG
#endif

static int get_debugfs_dir(char *debugfs_dir, size_t len)
{
    FILE *fp;
    char line[MAX_PATH + 1 + 256];

    fp = fopen("/proc/mounts","r");
    if (!fp) {
        log(LOG_INFO, "Can't open /proc/mounts");
        return errno;
    }

    do {
        char *p, *type, *dir;
        if (!fgets(line, sizeof(line), fp))
            break;

        p = strtok(line, " \t");
        if (!p)
            break;

        dir = strtok(NULL, " \t");
        if (!dir)
            break;

        type = strtok(NULL, " \t");
        if (!type)
            break;

        if (!strcmp(type, "debugfs")) {
            fclose(fp);
            strncpy(debugfs_dir, dir, len - 1);
            debugfs_dir[len - 1] = '\0';
            return 0;
        }
    } while(1);

    fclose(fp);
    log(LOG_INFO, "Can't find debugfs\n");
    return ENOENT;
}


static int open_trace(char *trace_dir, char *name, int flags)
{
    int ret;
    char fname[MAX_PATH + 1];

    strcpy(fname, trace_dir);
    strcat(fname, "/");
    strcat(fname, name);

    ret = open(fname, flags);
    if (ret < 0)
        log(LOG_WARNING, "open_trace() failed, fname=%s ret=%d errno=%d\n", fname, ret, errno);

    return ret;
}

static int create_trace_instance(char *trace_instance_dir)
{
    char fname[MAX_PATH + 1];
    int rc;

    get_debugfs_dir(fname, sizeof(fname));
    strcat(fname, "/tracing/instances/"TOOL_NAME);
    rc = mkdir(fname, S_IRWXU);
    if (rc < 0 && errno != EEXIST) {
        log(LOG_INFO, "Unable to create " TOOL_NAME " instance at %s\n", fname);
        return -1;
    }
    strcpy(trace_instance_dir, fname);
    return 0;
}

struct ras_events *init_trace_instance(void)
{
    struct ras_events *ras = calloc(1, sizeof(*ras));
    if (!ras) {
        log(LOG_ERROR, "Can't allocate memory for ras struct\n");
        return NULL;
    }
    int rc = create_trace_instance(ras->tracing);
    if (rc < 0) {
        free(ras);
        return NULL;
    }
    return ras;
}

/*
 * Tracing enable/disable code
 */
int toggle_ras_event(char *trace_dir, char *group, char *event, int enable)
{
    int fd, rc;
    char fname[MAX_PATH + 1];

    snprintf(fname, sizeof(fname), "%s%s:%s\n",
         enable ? "" : "!",
         group, event);

    /* Enable RAS events */
    fd = open_trace(trace_dir, "set_event", O_RDWR | O_APPEND);
    if (fd < 0) {
        log(LOG_WARNING, "Can't open set_event\n");
        rc = -errno;
        goto err;
    }

    rc = write(fd, fname, strlen(fname));
    close(fd);
    if (rc <= 0) {
        log(LOG_WARNING, "Can't write to set_event\n");
        rc = -EIO;
        goto err;
    }

    log(LOG_INFO, "%s:%s event %s\n",
        group, event,
        enable ? "enabled" : "disabled");
    return 0;
err:
    log(LOG_ERROR, "Can't %s %s:%s tracing\n",
        enable ? "enable" : "disable", group, event);
    return rc;
}

static int parse_header_page(struct ras_events *ras, struct tep_handle *pevent)
{
    int fd, len, page_size = DEFAULT_PAGE_SIZE;
    char buf[page_size];

    fd = open_trace(ras->tracing, "events/header_page", O_RDONLY);
    if (fd < 0) {
        log(LOG_WARNING, "Open event header page failed\n");
        return -1;
    }

    len = read(fd, buf, page_size);
    close(fd);
    if (len <= 0) {
        log(LOG_WARNING, "Read event header page failed\n");
        return -1;
    }

    if (tep_parse_header_page(pevent, buf, len, sizeof(long))) {
        log(LOG_WARNING, "Parse event header page failed\n");
        return -1;
    }

    return 0;
}

static void parse_ras_data(struct pcpu_data *pdata, struct kbuffer *kbuf,
               void *data, unsigned long long time_stamp)
{
    struct tep_record record;
    struct trace_seq s;

    record.ts = time_stamp;
    record.size = kbuffer_event_size(kbuf);
    record.data = data;
    record.offset = kbuffer_curr_offset(kbuf);
    record.cpu = pdata->cpu;

    /* note offset is just offset in subbuffer */
    record.missed_events = kbuffer_missed_events(kbuf);
    record.record_size = kbuffer_curr_size(kbuf);

    trace_seq_init(&s);
    tep_print_event(pdata->ras->pevent, &s, &record, "%s-%s-%d-%s", 
                    TEP_PRINT_NAME, TEP_PRINT_COMM, TEP_PRINT_TIME, TEP_PRINT_INFO);
    trace_seq_do_printf(&s);
    fflush(stdout);
    trace_seq_destroy(&s);
}

static int get_num_cpus()
{
    return sysconf(_SC_NPROCESSORS_ONLN);
}

static int set_buffer_percent(struct ras_events *ras, int percent)
{
    int res = 0;
    int fd;

    fd = open_trace(ras->tracing, "buffer_percent", O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        ssize_t size;
        snprintf(buf, sizeof(buf), "%d", percent);
        size = write(fd, buf, strlen(buf));
        if (size <= 0) {
            log(LOG_WARNING, "can't write to buffer_percent\n");
            res = -1;
        }
        close(fd);
    } else {
        log(LOG_WARNING, "Can't open buffer_percent\n");
        res = -1;
    }

    return res;
}

static int read_ras_event_all_cpus(struct pcpu_data *pdata,
                   unsigned n_cpus)
{
    ssize_t size;
    unsigned long long time_stamp;
    void *data;
    int ready, i, count_nready;
    struct kbuffer *kbuf;
    void *page;
    struct pollfd fds[n_cpus + 1];
    struct signalfd_siginfo fdsiginfo;
    sigset_t mask;
    int warnonce[n_cpus];
    char pipe_raw[PATH_MAX];

    memset(&warnonce, 0, sizeof(warnonce));

    page = malloc(pdata[0].ras->page_size);
    if (!page) {
        log(LOG_ERROR, "Can't allocate page\n");
        return -ENOMEM;
    }

    kbuf = kbuffer_alloc(KBUFFER_LSIZE_8, ENDIAN);
    if (!kbuf) {
        log(LOG_ERROR, "Can't allocate kbuf\n");
        free(page);
        return -ENOMEM;
    }

    /* Fix for poll() on the per_cpu trace_pipe and trace_pipe_raw blocks
     * indefinitely with the default buffer_percent in the kernel trace system,
     * which is introduced by the following change in the kernel.
     * https://lore.kernel.org/all/20221020231427.41be3f26@gandalf.local.home/T/#u.
     * Set buffer_percent to 0 so that poll() will return immediately
     * when the trace data is available in the ras per_cpu trace pipe_raw
     */
    if (set_buffer_percent(pdata[0].ras, 0))
        log(LOG_WARNING, "Set buffer_percent failed\n");

    for (i = 0; i < (n_cpus + 1); i++)
        fds[i].fd = -1;

    for (i = 0; i < n_cpus; i++) {
        fds[i].events = POLLIN;

        snprintf(pipe_raw, sizeof(pipe_raw),
            "per_cpu/cpu%d/trace_pipe_raw", i);

        fds[i].fd = open_trace(pdata[0].ras->tracing, pipe_raw, O_RDONLY);
        if (fds[i].fd < 0) {
            log(LOG_ERROR, "Can't open trace_pipe_raw\n");
            goto error;
        }
    }

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        log(LOG_WARNING, "sigprocmask\n");
    fds[n_cpus].events = POLLIN;
    fds[n_cpus].fd = signalfd(-1, &mask, 0);
    if (fds[n_cpus].fd < 0) {
        log(LOG_WARNING, "signalfd\n");
        goto error;
    }

    log(LOG_INFO, "Listening to events for cpus 0 to %u\n", n_cpus - 1);

    do {
        ready = poll(fds, (n_cpus + 1), -1);
        if (ready < 0) {
            log(LOG_WARNING, "poll\n");
        }

        /* check for the signal */
        if (fds[n_cpus].revents & POLLIN) {
            size = read(fds[n_cpus].fd, &fdsiginfo,
                    sizeof(struct signalfd_siginfo));
            if (size != sizeof(struct signalfd_siginfo)) {
                log(LOG_WARNING, "signalfd read\n");
                continue;
            }

            if (fdsiginfo.ssi_signo == SIGINT ||
                fdsiginfo.ssi_signo == SIGTERM ||
                fdsiginfo.ssi_signo == SIGHUP ||
                fdsiginfo.ssi_signo == SIGQUIT) {
                log(LOG_INFO, "Recevied signal=%d\n",
                    fdsiginfo.ssi_signo);
                goto error;
            } else {
                log(LOG_INFO,
                    "Received unexpected signal=%d\n",
                    fdsiginfo.ssi_signo);
                continue;
            }
        }

        count_nready = 0;
        for (i = 0; i < n_cpus; i++) {
            if (fds[i].revents & POLLERR) {
                if (!warnonce[i]) {
                    log(LOG_INFO,
                        "Error on CPU %i\n", i);
                    warnonce[i]++;
                }
                continue;
            }
            if (!(fds[i].revents & POLLIN)) {
                count_nready++;
                continue;
            }
            size = read(fds[i].fd, page, pdata[i].ras->page_size);
            if (size < 0) {
                log(LOG_WARNING, "read\n");
                goto error;
            } else if (size > 0) {
                log(LOG_DEBUG, "cpu %d receive %ld bytes data\n", i, size);
                kbuffer_load_subbuffer(kbuf, page);

                while ((data = kbuffer_read_event(kbuf, &time_stamp))) {
                    if (kbuffer_curr_size(kbuf) < 0) {
                        log(LOG_ERROR, "invalid kbuf data, discard\n");
                        break;
                    }

                    log(LOG_DEBUG, "parse_ras_data\n");
                    parse_ras_data(&pdata[i],
                               kbuf, data, time_stamp);

                    /* increment to read next event */
                    log(LOG_DEBUG, "kbuffer_next_event\n");
                    kbuffer_next_event(kbuf, NULL);
                }
            } else {
                count_nready++;
            }
        }

        /*
         * If count_nready == n_cpus, there is no cpu fd in POLLIN state,
         * so we need to break the cycle
         */
        if (count_nready == n_cpus) {
            log(LOG_ERROR, "no cpu fd in POLLIN state, stop running\n");
            break;
        }
    } while (1);

error:
    kbuffer_free(kbuf);
    free(page);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    for (i = 0; i < (n_cpus + 1); i++) {
        if (fds[i].fd > 0)
            close(fds[i].fd);
    }

    return -1;
}

static int init_header_page(struct ras_events *ras, struct tep_handle *pevent)
{
    int rc;

    rc = parse_header_page(ras, pevent);
    if (rc) {
        log(LOG_ERROR, "cannot read trace header_page: %d\n", rc);
        return rc;
    }
    return 0;
}

static int init_event_format(struct ras_events *ras, struct tep_handle *pevent,
                 char *group, char *event)
{
    char *page, fname[MAX_PATH + 1];
    int fd, size, rc, page_size = DEFAULT_PAGE_SIZE;

    // read one page from format
    snprintf(fname, sizeof(fname), "events/%s/%s/format", group, event);
    fd = open_trace(ras->tracing, fname, O_RDONLY);
    if (fd < 0) {
        log(LOG_ERROR,
            "Can't get %s:%s traces. Perhaps this feature is not supported on your system.\n",
            group, event);
        return errno;
    }

    log(LOG_INFO, "page_size: %d\n", page_size);
    ras->page_size = page_size;
    page = malloc(page_size);
    if (!page) {
        log(LOG_ERROR, "Can't allocate page to read %s:%s format\n",
            group, event);
        rc = errno;
        close(fd);
        return rc;
    }

    size = read(fd, page, page_size);
    close(fd);
    if (size < 0) {
        log(LOG_ERROR, "Can't read format\n");
        free(page);
        return size;
    }

    // parse event format
    rc = tep_parse_event(pevent, page, size, group);
    if (rc) {
        log(LOG_ERROR, "Can't parse event %s:%s\n", group, event);
        free(page);
        return EINVAL;
    }
    return 0;
}

static int add_event_handler(struct ras_events *ras, struct tep_handle *pevent,
                 char *group, char *event,
                 tep_event_handler_func func)
{
    int rc;

    rc = init_event_format(ras, pevent, group, event);
    if (rc) {
        log(LOG_ERROR, "init_event_format for %s:%s failed\n", group, event);
        return rc;
    }

    /* Registers the special event handlers */
    rc = tep_register_event_handler(pevent, -1, group, event, func, ras);
    if (rc < 0) {
        log(LOG_ERROR, "Can't register event handler for %s:%s\n",
            group, event);
        return EINVAL;
    }

    return 0;
}

int handle_ras_events(struct ras_events *ras)
{
    int rc, i;
    unsigned cpus;
    struct tep_handle *pevent = NULL;
    struct pcpu_data *data = NULL;

    pevent = tep_alloc();
    if (!pevent) {
        log(LOG_ERROR, "Can't allocate pevent\n");
        rc = errno;
        goto err;
    }
    ras->pevent = pevent;

    rc = init_header_page(ras, pevent);
    if (rc) {
        log(LOG_ERROR, "init_header_page failed\n");
        goto err;
    }

    rc = add_event_handler(ras, pevent, "ras", "non_standard_event",
                ras_non_standard_event_handler);
    if (rc) {
        log(LOG_ERROR, "Can't get traces from %s:%s\n",
            "ras", "non_standard_event");
        goto err;
    }
    log(LOG_INFO, "add_event_handler done\n");

    cpus = get_num_cpus();
    data = calloc(sizeof(*data), cpus);
    if (!data)
        goto err;

    for (i = 0; i < cpus; i++) {
        data[i].ras = ras;
        data[i].cpu = i;
    }
    rc = read_ras_event_all_cpus(data, cpus);

err:
    if (data)
        free(data);
    if (pevent)
        tep_free(pevent);
    return rc;
}
