all: soc_ring_sentry

soc_ring_sentry:
	gcc *.c -Wall -o3 -I../../libso/log -L../../libso/log/build -I../../libso/xalarm -L../../libso/build/xalarm -lxalarm -lsentry_log -ljson-c -pthread -lnuma -o soc_ring_sentry

clean:
	rm -f soc_ring_sentry

.PHONY: all clean
