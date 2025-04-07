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
LOGROTEDIR	= $(DESTDIR)$(ETCDIR)/logrotate-sysSentry.conf

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

PYTHON_VERSION := $(shell $(PYBIN) --version 2>&1 | awk '{print $$2}'| cut -d '.' -f 1,2)
PKGVER := syssentry-$(VERSION)-py$(PYTHON_VERSION)
PKGVEREGG := syssentry-$(VERSION)-py$(PYTHON_VERSION).egg-info

ifeq ($(shell uname -r | grep -q "5.10" && echo 1 || echo 0), 1)
	KERNEL_IS_5_10 := 1
else
	KERNEL_IS_5_10 := 0
endif

all: lib sentry hbm_online_repair cpu_patrol


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
	@if [ $(KERNEL_IS_5_10) -eq 1 ]; then \
		cd $(CURSRCDIR)/c/ebpf_collector/ && make; \
	else \
        	echo "Kernel is not 5.10, skipping building ebpf_collector"; \
	fi

isentry:
	cd $(CURSRCDIR)/python/ && $(PYBIN) setup.py install -O1 --root=build/ --record=SENTRY_FILES
	find $(CURSRCDIR)/python/build/ -type d -name '__pycache__' -exec rm -rf {} +
	install -m 600 $(CURCONFIGDIR)/logrotate-sysSentry.conf $(LOGROTEDIR)/
	install -m 600 $(CURCONFIGDIR)/inspect.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/xalarm.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/collector.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURSERVICEDIR)/sysSentry.service $(SYSTEMDDIR)
	install -m 600 $(CURSERVICEDIR)/xalarmd.service $(SYSTEMDDIR)
	install -m 600 $(CURSERVICEDIR)/sentryCollector.service $(SYSTEMDDIR)
	install -d -m 550 $(PYDIR)/syssentry
	install -d -m 550 $(PYDIR)/xalarm
	install -d -m 550 $(PYDIR)/sentryCollector
	install -d -m 550 $(PYDIR)/$(PKGVEREGG)
	install -m 550 src/python/build/usr/local/lib/$(PYNAME)/site-packages/syssentry/* $(PYDIR)/syssentry
	install -m 550 src/python/build/usr/local/lib/$(PYNAME)/site-packages/xalarm/* $(PYDIR)/xalarm
	install -m 550 src/python/xalarm/register_xalarm.py $(PYDIR)/xalarm
	install -m 550 src/python/build/usr/local/lib/$(PYNAME)/site-packages/sentryCollector/* $(PYDIR)/sentryCollector
	install -m 550 src/python/build/usr/local/lib/$(PYNAME)/site-packages/$(PKGVEREGG)/* $(PYDIR)/$(PKGVEREGG)
	install -m 550 $(CURSRCDIR)/python/syssentry/sentryctl $(BINDIR)
	install -m 550 $(CURSRCDIR)/python/build/usr/local/bin/syssentry $(BINDIR)
	install -m 550 $(CURSRCDIR)/python/build/usr/local/bin/xalarmd $(BINDIR)
	install -m 550 $(CURSRCDIR)/python/build/usr/local/bin/sentryCollector $(BINDIR)
	@if [ -f "$(CURSRCDIR)/sentryCollector/ebpf_collector/ebpf_collector" ]; then \
		install -m 550 $(CURSRCDIR)/sentryCollector/ebpf_collector/ebpf_collector $(BINDIR); \
	fi

	# ai_block_io
	install -d -m 700 $(PYDIR)/sentryPlugins/ai_block_io
	install -m 550 $(CURSRCDIR)/python/build/usr/local/bin/ai_block_io $(BINDIR)
	install -m 550 src/python/build/usr/local/lib/$(PYNAME)/site-packages/sentryPlugins/ai_block_io/* $(PYDIR)/sentryPlugins/ai_block_io
	install -m 600 $(CURCONFIGDIR)/plugins/ai_block_io.ini $(ETCDIR)/sysSentry/plugins
	install -m 600 $(CURCONFIGDIR)/tasks/ai_block_io.mod $(ETCDIR)/sysSentry/tasks

	# cpu_patrol
	install -m 550 $(CURSRCDIR)/c/catcli/catlib/build/cat-cli $(BINDIR)
	install -m 550 $(CURSRCDIR)/c/catcli/catlib/build/plugin/cpu_patrol/libcpu_patrol.so $(LIBINSTALLDIR)
	install -m 600 $(CURCONFIGDIR)/plugins/cpu_sentry.ini $(ETCDIR)/sysSentry/plugins
	install -m 600 $(CURCONFIGDIR)/tasks/cpu_sentry.mod $(ETCDIR)/sysSentry/tasks

	# hbm_online_repair
	install -d -m 700 $(ETCDIR)/sysconfig
	install -m 550 $(CURSRCDIR)/c/hbm_online_repair/hbm_online_repair $(BINDIR)
	install -m 550 $(CURSRCDIR)/c/hbm_online_repair/hbm_online_repair.env $(ETCDIR)/sysconfig
	install -m 600 $(CURCONFIGDIR)/tasks/hbm_online_repair.mod $(ETCDIR)/sysSentry/tasks

lib:
	cd $(CURSRCDIR)/libso && cmake . -DXD_INSTALL_BINDIR=$(LIBINSTALLDIR) -B build
	cd $(CURSRCDIR)/libso/build && make

ilib:
	cd $(CURSRCDIR)/libso/build && make install
	install -d -m 644 $(INCLUDEDIR)/xalarm
	install -m 644 $(CURSRCDIR)/libso/xalarm/register_xalarm.h $(INCLUDEDIR)/xalarm/

cpu_patrol:
	cd $(CURSRCDIR)/c/catcli/catlib/ && cmake -B ./build/ -S . -DCMAKE_INSTALL_PREFIX=/usr/local
	cd $(CURSRCDIR)/c/catcli/catlib/build && make

hbm_online_repair:
	cd $(CURSRCDIR)/c/hbm_online_repair/ && make

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
	rm -rf $(LOGROTEDIR)/
	rm -rf $(LOGSAVEDIR)/sysSentry
	rm -rf $(PYDIR)/syssentry
	rm -rf $(PYDIR)/xalarm
	rm -rf $(PYDIR)/sentryCollector
	rm -rf $(PYDIR)/$(PKGVEREGG)
	rm -rf $(PYDIR)/sentryPlugins
	rm -rf $(SYSTEMDDIR)/sysSentry.service
	rm -rf $(SYSTEMDDIR)/xalarmd.service
	rm -rf $(SYSTEMDDIR)/sentryCollector.service

	# clean cpu_patrol
	rm -rf $(BINDIR)/cat-cli
	rm -rf $(LIBINSTALLDIR)/libcpu_patrol.so

	# clean hbm_online_repair
	rm -rf $(BINDIR)/hbm_online_repair
	rm -rf $(ETCDIR)/sysconfig/hbm_online_repair.env
	
	systemctl daemon-reload

test:
	@if [[ -z "$(log)" ]]; then \
		log="console";  \
	fi
	@if [[ -z "$t" ]]; then \
		cd  $(CURTESTDIR) && sh ./test.sh "all" $(log); \
	else \
		cd  $(CURTESTDIR) && sh ./test.sh $t $(log); \
	fi

startup:
	systemctl daemon-reload
	systemctl restart xalarmd
	systemctl restart sysSentry
	systemctl restart sentryCollector
