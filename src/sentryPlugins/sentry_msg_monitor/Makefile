all: sentry_msg_monitor

sentry_msg_monitor:
	gcc sentry_msg_monitor.c -Wall -o3 -I../../libsentry/c/log -L../../libsentry/c/log/build -I../../libs/libxalarm -L../../libs/build/libxalarm -lxalarm -lsentry_log -ljson-c -pthread -o sentry_msg_monitor

clean:
	rm -f sentry_msg_monitor

.PHONY: all clean
