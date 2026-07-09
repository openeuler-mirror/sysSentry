CFLAGS += -Wall -O2 -g -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -Wtrampolines
LDFLAGS += -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pie

all: soc_ring_sentry

soc_ring_sentry:
	gcc *.c $(CFLAGS) -I../../libsentry/c/log -L../../libsentry/c/log/build -I../../libs/libxalarm -L../../libs/build/libxalarm -lxalarm -lsentry_log -ljson-c -pthread -lnuma $(LDFLAGS) -o soc_ring_sentry

clean:
	rm -f soc_ring_sentry

.PHONY: all clean
