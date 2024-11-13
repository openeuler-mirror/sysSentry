VERSION = 1.0.2
.PHONY: all install test

PREFIX	?= /usr
ETCDIR	?= /etc
VARDIR	?= /var
BINDIR	= $(DESTDIR)$(PREFIX)/bin
SYSTEMDDIR	= $(DESTDIR)$(PREFIX)/lib/systemd/system
INCLUDEDIR      = $(DESTDIR)$(PREFIX)/include
LIBINSTALLDIR   = $(DESTDIR)$(PREFIX)/lib64
LOGSAVEDIR	= $(DESTDIR)$(VARDIR)/log/sysSentry
LOGROTEDIR	= $(DESTDIR)$(ETCDIR)/logrotate.d

SRCVERSION = $(shell git rev-parse --short HEAD 2>/dev/null)
SHELL = /bin/bash

CURDIR	= $(shell pwd)
CURSRCDIR	= $(CURDIR)/src
CURLIBDIR	= $(CURDIR)/libs
CURTESTDIR	= $(CURDIR)/selftest
CURSERVICEDIR	= $(CURDIR)/service
CURCONFIGDIR	= $(CURDIR)/config

PYBIN	= $(shell which python3)
PYNAME	= $(shell ls /usr/lib |grep -E '^python'| sort -V | tail -n1)
PYDIR	= $(DESTDIR)$(PREFIX)/lib/$(PYNAME)/site-packages

ifeq ($(shell uname -r | grep -q "4.19" && echo 1 || echo 0), 1)
	KERNEL_IS_4_19 := 1
else
	KERNEL_IS_4_19 :=
endif

all: lib sentry


install: dirs ilib isentry


dirs:
	mkdir -p $(BINDIR)
	mkdir -p $(ETCDIR)
	mkdir -p $(PYDIR)
	mkdir -p $(SYSTEMDDIR)
	mkdir -p $(LOGSAVEDIR)
	mkdir -p $(LOGROTEDIR)
	install -d -m 700 $(ETCDIR)/sysSentry/
	install -d -m 700 $(ETCDIR)/sysSentry/tasks/
	install -d -m 700 $(ETCDIR)/sysSentry/plugins/

sentry:
	@echo "Kernel version is $(shell uname -r)"
	@if [ $(KERNEL_IS_4_19) -eq 1 ]; then \
		cd $(CURSRCDIR)/sentryCollector/ebpf_collector/ && make; \
	else \
        	echo "Kernel is not 4.19, skipping building ebpf_collector"; \
	fi

isentry:
	cd $(CURSRCDIR) && $(PYBIN) setup.py install -O1 --root=build/ --record=SENTRY_FILES
	find $(CURSRCDIR)/build/ -type d -name '__pycache__' -exec rm -rf {} +
	install -m 600 $(CURCONFIGDIR)/logrotate $(LOGROTEDIR)/sysSentry
	install -m 600 $(CURCONFIGDIR)/inspect.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/xalarm.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/collector.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURSERVICEDIR)/sysSentry.service $(SYSTEMDDIR)
	install -m 600 $(CURSERVICEDIR)/xalarmd.service $(SYSTEMDDIR)
	install -m 600 $(CURSERVICEDIR)/sentryCollector.service $(SYSTEMDDIR)
	install -d -m 550 $(PYDIR)/syssentry
	install -d -m 550 $(PYDIR)/xalarm
	install -d -m 550 $(PYDIR)/sentryCollector
	install -m 550 src/build/usr/local/lib/$(PYNAME)/site-packages/syssentry/* $(PYDIR)/syssentry
	install -m 550 src/build/usr/local/lib/$(PYNAME)/site-packages/xalarm/* $(PYDIR)/xalarm
	install -m 550 libs/pyxalarm/register_xalarm.py $(PYDIR)/xalarm
	install -m 550 src/build/usr/local/lib/$(PYNAME)/site-packages/sentryCollector/* $(PYDIR)/sentryCollector
	install -m 550 $(CURSRCDIR)/syssentry/sentryctl $(BINDIR)
	install -m 550 $(CURSRCDIR)/build/usr/local/bin/syssentry $(BINDIR)
	install -m 550 $(CURSRCDIR)/build/usr/local/bin/xalarmd $(BINDIR)
	install -m 550 $(CURSRCDIR)/build/usr/local/bin/sentryCollector $(BINDIR)
	@if [ -f "$(CURSRCDIR)/sentryCollector/ebpf_collector/ebpf_collector" ]; then \
		install -m 550 $(CURSRCDIR)/sentryCollector/ebpf_collector/ebpf_collector $(BINDIR); \
	fi

lib:
	cd $(CURLIBDIR) && cmake . -DXD_INSTALL_BINDIR=$(LIBINSTALLDIR) -B build
	cd $(CURLIBDIR)/build && make

ilib:
	cd $(CURLIBDIR)/build && make install
	install -d -m 644 $(INCLUDEDIR)/xalarm
	install -m 644 $(CURLIBDIR)/libxalarm/register_xalarm.h $(INCLUDEDIR)/xalarm/


clean-install:
	rm -rf $(CURLIBDIR)/build
	rm -rf $(CURSRCDIR)/build
	rm -rf $(CURSRCDIR)/sentryCollector/ebpf_collector/ebpf_collector
	rm -rf $(CURSRCDIR)/sentryCollector/ebpf_collector/output
	rm -rf $(CURSRCDIR)/syssentry.egg-info
	rm -rf $(CURSRCDIR)/SENTRY_FILES

clean: clean-install
	rm -rf $(BINDIR)/sentryctl
	rm -rf $(BINDIR)/syssentry
	rm -rf $(BINDIR)/xalarmd
	rm -rf $(BINDIR)/sentryCollector
	rm -rf $(BINDIR)/ebpf_collector
	rm -rf $(LIBINSTALLDIR)/libxalarm.so
	rm -rf $(INCLUDEDIR)/xalarm
	rm -rf $(ETCDIR)/sysSentry
	rm -rf $(LOGROTEDIR)/sysSentry
	rm -rf $(LOGSAVEDIR)/sysSentry
	rm -rf $(PYDIR)/syssentry
	rm -rf $(PYDIR)/xalarm
	rm -rf $(PYDIR)/sentryCollector
	rm -rf $(SYSTEMDDIR)/sysSentry.service
	rm -rf $(SYSTEMDDIR)/xalarmd.service
	rm -rf $(SYSTEMDDIR)/sentryCollector.service
	systemctl daemon-reload

test:

startup:
	systemctl daemon-reload
	systemctl restart xalarmd
	systemctl restart sysSentry
	systemctl restart sentryCollector
