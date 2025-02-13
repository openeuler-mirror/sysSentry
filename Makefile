VERSION = 1.0.3
.PHONY: all install test

PREFIX          = /usr
ETCDIR          = $(DESTDIR)/etc
VARDIR          = $(DESTDIR)/var
BINDIR          = $(DESTDIR)$(PREFIX)/bin
SYSTEMDDIR      = $(DESTDIR)$(PREFIX)/lib/systemd/system
INCLUDEDIR      = $(DESTDIR)$(PREFIX)/include
LIBINSTALLDIR   = $(DESTDIR)$(PREFIX)/lib64
LOGSAVEDIR      = $(VARDIR)/log
LOGROTEDIR      = $(ETCDIR)/logrotate-sysSentry.conf
LOGCRON         = $(ETCDIR)/cron.hourly
VARLIB          = $(VARDIR)/lib

CURDIR          = $(shell pwd)
CURSRCDIR       = $(CURDIR)/src
CURLIBDIR       = $(CURDIR)/src/libs
CURTESTDIR      = $(CURDIR)/selftest
CURCONFIGDIR    = $(CURDIR)/config

PYBIN   = $(shell which python3)
PYNAME  = $(shell ls /usr/lib |grep -E '^python'| sort -V | tail -n1)
PYDIR   = $(DESTDIR)$(PREFIX)/lib/$(PYNAME)/site-packages

PYTHON_VERSION := $(shell $(PYBIN) --version 2>&1 | awk '{print $$2}' | cut -d '.' -f 1,2)
PKGVER := syssentry-$(VERSION)-py$(PYTHON_VERSION)
PKGVEREGG := syssentry-$(VERSION)-py$(PYTHON_VERSION).egg-info

all: lib ebpf hbm_online_repair

lib:libxalarm

libxalarm:
	cd $(CURLIBDIR) && cmake . -DXD_INSTALL_BINDIR=$(LIBINSTALLDIR) -B build
	cd $(CURLIBDIR)/build && make

ebpf:
	@if [ -d "$(CURSRCDIR)/services/sentryCollector/ebpf_collector/" ]; then \
		cd $(CURSRCDIR)/services/sentryCollector/ebpf_collector/ && make;	\
	fi

hbm_online_repair:
	cd $(CURSRCDIR)/sentryPlugins/hbm_online_repair/ && make

install: all dirs isentry

dirs:
	mkdir -p $(BINDIR)
	mkdir -p $(ETCDIR)
	mkdir -p $(PYDIR)
	mkdir -p $(SYSTEMDDIR)
	mkdir -p $(LOGCRON)
	mkdir -p $(LIBINSTALLDIR)
	mkdir -p $(ETCDIR)/sysSentry/
	install -d -m 700 $(ETCDIR)/sysSentry/
	install -d -m 700 $(ETCDIR)/sysSentry/tasks/
	install -d -m 700 $(ETCDIR)/sysSentry/plugins/

isentry:
	cd $(CURSRCDIR) && $(PYBIN) setup.py install --prefix=$(PREFIX) -O1 --root=build --record=SENTRY_FILES
	find $(CURSRCDIR)/build/ -type d -name '__pycache__' -exec rm -rf {} +
	
	# sysSentry主包
	## 创建目录
	install -d -m 700 $(LOGSAVEDIR)/sysSentry
	install -d -m 700 $(VARLIB)/logrotate-syssentry
	install -d -m 700 $(PYDIR)/syssentry
	install -d -m 700 $(PYDIR)/xalarm
	install -d -m 700 $(PYDIR)/sentryCollector
	install -d -m 700 $(PYDIR)/$(PKGVEREGG)
	install -d -m 700 $(ETCDIR)/sysconfig
	
	## 安装配置文件
	install -m 600 $(CURCONFIGDIR)/logrotate-sysSentry.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/inspect.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/xalarm.conf $(ETCDIR)/sysSentry/
	install -m 600 $(CURCONFIGDIR)/collector.conf $(ETCDIR)/sysSentry/
	
	## 安装日志文件
	install -m 0500 src/libsentry/sh/log/logrotate-sysSentry.cron $(ETCDIR)/cron.hourly/logrotate-sysSentry
	
	## 安装 systemd 服务文件
	install -m 600 $(CURCONFIGDIR)/service/sysSentry.service $(SYSTEMDDIR)
	install -m 600 $(CURCONFIGDIR)/service/xalarmd.service $(SYSTEMDDIR)
	install -m 600 $(CURCONFIGDIR)/service/sentryCollector.service $(SYSTEMDDIR)
	
	## 安装python源代码文件到相应的目录
	install -m 550 src/build/usr/lib/$(PYNAME)/site-packages/services/syssentry/* $(PYDIR)/syssentry
	install -m 550 src/build/usr/lib/$(PYNAME)/site-packages/services/xalarm/* $(PYDIR)/xalarm
	install -m 550 src/build/usr/lib/$(PYNAME)/site-packages/services/sentryCollector/* $(PYDIR)/sentryCollector
	install -m 550 src/build/usr/lib/$(PYNAME)/site-packages/$(PKGVEREGG)/* $(PYDIR)/$(PKGVEREGG)
	
	## 安装可执行文件
	install -m 550 $(CURSRCDIR)/services/syssentry/sentryctl $(BINDIR)
	install -m 550 $(CURSRCDIR)/build/usr/bin/syssentry $(BINDIR)
	install -m 550 $(CURSRCDIR)/build/usr/bin/xalarmd $(BINDIR)
	install -m 550 $(CURSRCDIR)/build/usr/bin/sentryCollector $(BINDIR)
	@if [ -f "$(CURSRCDIR)/services/sentryCollector/ebpf_collector/ebpf_collector" ]; then \
		install -m 550 $(CURSRCDIR)/services/sentryCollector/ebpf_collector/ebpf_collector $(BINDIR); \
	fi
	
	# avg_block_io
	install -d -m 700 $(PYDIR)/sentryPlugins/avg_block_io
	install -m 550 $(CURSRCDIR)/build/usr/bin/avg_block_io $(BINDIR)
	install -m 550 src/build/usr/lib/$(PYNAME)/site-packages/sentryPlugins/avg_block_io/* $(PYDIR)/sentryPlugins/avg_block_io
	install -m 600 $(CURCONFIGDIR)/plugins/avg_block_io.ini $(ETCDIR)/sysSentry/plugins/
	install -m 600 $(CURCONFIGDIR)/tasks/avg_block_io.mod $(ETCDIR)/sysSentry/tasks/

	# ai_block_io
	install -d -m 700 $(PYDIR)/sentryPlugins/ai_block_io
	install -m 550 $(CURSRCDIR)/build/usr/bin/ai_block_io $(BINDIR)
	install -m 550 src/build/usr/lib/$(PYNAME)/site-packages/sentryPlugins/ai_block_io/* $(PYDIR)/sentryPlugins/ai_block_io
	install -m 600 $(CURCONFIGDIR)/plugins/ai_block_io.ini $(ETCDIR)/sysSentry/plugins/
	install -m 600 $(CURCONFIGDIR)/tasks/ai_block_io.mod $(ETCDIR)/sysSentry/tasks/

	# hbm_online_repair
	install -m 550 $(CURSRCDIR)/sentryPlugins/hbm_online_repair/hbm_online_repair $(BINDIR)
	install -m 600 $(CURCONFIGDIR)/env/hbm_online_repair.env $(ETCDIR)/sysconfig/
	install -m 600 $(CURCONFIGDIR)/tasks/hbm_online_repair.mod $(ETCDIR)/sysSentry/tasks/

	# pysentry_notify
	install -m 550 src/libsentry/python/pySentryNotify/sentry_notify.py $(PYDIR)/xalarm

	# pysentry_collect
	install -m 550 src/libsentry/python/pySentryCollector/collect_plugin.py $(PYDIR)/sentryCollector

	# libxalarm
	install -m 550 $(CURLIBDIR)/build/libxalarm/libxalarm.so $(LIBINSTALLDIR)

	# libxalarm-devel
	install -d -m 700 $(INCLUDEDIR)/xalarm
	install -m 644 $(CURLIBDIR)/libxalarm/register_xalarm.h $(INCLUDEDIR)/xalarm/

	# pyxalarm
	install -m 550 src/libs/pyxalarm/register_xalarm.py $(PYDIR)/xalarm

ebpf_clean:
	cd $(CURSRCDIR)/services/sentryCollector/ebpf_collector && make clean

hbm_clean:
	cd $(CURSRCDIR)/sentryPlugins/hbm_online_repair && make clean

clean: ebpf_clean hbm_clean
	rm -rf $(CURLIBDIR)/build
	rm -rf $(CURSRCDIR)/build
	rm -rf $(CURSRCDIR)/syssentry.egg-info
	rm -rf $(CURSRCDIR)/SENTRY_FILES

uninstall:
	rm -rf $(BINDIR)/sentryctl
	rm -rf $(BINDIR)/syssentry
	rm -rf $(BINDIR)/xalarmd
	rm -rf $(BINDIR)/sentryCollector
	rm -rf $(BINDIR)/hbm_online_repair
	rm -rf $(BINDIR)/ebpf_collector
	rm -rf $(LIBINSTALLDIR)/libxalarm.so
	rm -rf $(INCLUDEDIR)/xalarm
	rm -rf $(ETCDIR)/sysSentry
	rm -rf $(ETCDIR)/hbm_online_repair.env
	rm -rf $(LOGSAVEDIR)/sysSentry
	rm -rf $(PYDIR)/syssentry
	rm -rf $(PYDIR)/xalarm
	rm -rf $(PYDIR)/sentryPlugins
	rm -rf $(PYDIR)/sentryCollector
	rm -rf $(PYDIR)/$(PKGVEREGG)
	rm -rf $(PYDIR)/xalarm
	rm -rf $(SYSTEMDDIR)/sysSentry.service
	rm -rf $(SYSTEMDDIR)/xalarmd.service
	rm -rf $(SYSTEMDDIR)/sentryCollector.service
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
