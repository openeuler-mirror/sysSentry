# Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
# Description: ebpf collector program
ARCH ?= $(shell uname -m | sed 's/x86_64/x86/' \
			 | sed 's/arm.*/arm/' \
			 | sed 's/aarch64/arm64/' \
			 | sed 's/ppc64le/powerpc/' \
			 | sed 's/mips.*/mips/' \
			 | sed 's/riscv64/riscv/' \
			 | sed 's/loongarch64/loongarch/')

KERNEL_VERSION ?= $(shell uname -r)
KERNEL_SRC := /usr/src/kernels/$(KERNEL_VERSION)
KERNEL_PATH := /usr/src/linux-$(shell rpm -qa kernel-source | cut -d' ' -f1 | sed 's/kernel-source-//')
GCC_ARCH ?= $(shell gcc -dumpmachine)
GCC_VERSION ?= $(shell gcc -dumpversion)

LINUX_INCLUDE := -I$(KERNEL_SRC)/include/
LINUX_INCLUDE += -I$(KERNEL_SRC)/arch/$(ARCH)/include/
LINUX_INCLUDE += -I$(KERNEL_SRC)/arch/$(ARCH)/include/generated
LINUX_INCLUDE += -I$(KERNEL_SRC)/arch/$(ARCH)/include/uapi
LINUX_INCLUDE += -I$(KERNEL_SRC)/arch/$(ARCH)/include/uapi/linux
LINUX_INCLUDE += -I$(KERNEL_SRC)/arch/$(ARCH)/include/generated/uapi
LINUX_INCLUDE += -I$(KERNEL_SRC)/include/uapi
LINUX_INCLUDE += -I$(KERNEL_SRC)/include/generated/uapi
LINUX_INCLUDE += -include $(KERNEL_SRC)/include/linux/kconfig.h
LINUX_INCLUDE += -I$(KERNEL_PATH)/samples/bpf
LINUX_INCLUDE += -I$(KERNEL_SRC)/tools/lib/
LINUX_INCLUDE += -I/usr/src/kernels/$(KERNEL_VERSION)/samples/bpf
LINUX_INCLUDE += -I$(KERNEL_SRC)/tools/perf/include/bpf
LINUX_INCLUDE += -I/usr/include/libbpf/src/bpf
LINUX_INCLUDE += -I/usr/src/kernels/$(KERNEL_VERSION)/include/uapi/linux/
LINUX_INCLUDE += -I/usr/include/bpf/
LINUX_INCLUDE += -I/usr/include/
BPF_LOAD_INCLUDE := -I/usr/include
BPF_LOAD_INCLUDE += -I$(KERNEL_SRC)/include/
BPF_LOAD_INCLUDE += -I/usr/src/kernels/$(KERNEL_VERSION)/include/
KBUILD_HOSTCFLAGS := -I$(KERNEL_PATH)/include/
KBUILD_HOSTCFLAGS += -I$(KERNEL_PATH)/tools/lib/ -I$(KERNEL_PATH)/tools/include
KBUILD_HOSTCFLAGS += -I$(KERNEL_PATH)/tools/perf
NOSTDINC_FLAGS := -nostdinc
EXTRA_CFLAGS := -isystem /usr/lib/gcc/$(GCC_ARCH)/$(GCC_VERSION)/include
CFLAGS := -g -Wall -w

CLANG_BPF_SYS_INCLUDES ?= $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

APPS = ebpf_collector

CC = gcc
LLC ?= llc
CLANG ?= clang

USER_CFLAGS = -I. -I/usr/src/kernels/$(KERNEL_VERSION)/include/uapi/linux/ -I/usr/src/kernel/include -Wall
KERNEL_CFLAGS = -I. -I/usr/src/kernels/$(KERNEL_VERSION)/include/uapi/linux/ -Wall 
LOADER_CFLAGS = -I. -I/usr/src/kernels/$(KERNEL_VERSION)/include/uapi/linux/ -I/usr/src/kernel/include
CLANG_FLAGS = -O2 -emit-llvm -c
LLC_FLAGS = -march=bpf -filetype=obj

OUTPUT := output

.PHONY: all
all: $(APPS)

.PHONY: clean
clean:
	$(call msg,CLEAN)
	$(Q)rm -rf $(OUTPUT) $(APPS)

$(OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(OUTPUT)/%.bpf.o: %.bpf.c
	$(call msg,BPF,$@)
	$(CLANG) $(NOSTDINC_FLAGS) $(EXTRA_CFLAGS) $(LINUX_INCLUDE) $(KBUILD_HOSTCFLAGS) \
		-D__KERNEL__ -D__BPF_TRACING__ -Wno-unused-value -Wno-pointer-sign \
		-D__TARGET_ARCH_$(ARCH) -Wno-compare-distinct-pointer-types \
		-Wno-gnu-variable-sized-type-not-at-end \
		-Wno-address-of-packed-member -Wno-tautological-compare \
		-Wno-unknown-warning-option $(CLANG_ARCH_ARGS) \
		-O2 -emit-llvm -c $< -o -| $(LLC) $(LLC_FLAGS) -o $@

$(patsubst %,$(OUTPUT)/%.o,$(APPS)): %.o: %.bpf.o

$(OUTPUT)/bpf_load.o: bpf_load.c | $(OUTPUT)
	$(call msg,CC,$@)
	$(CC) $(NOSTDINC_FLAGS) $(EXTRA_CFLAGS) $(CFLAGS) -I$(KERNEL_PATH)/samples/bpf -I$(KERNEL_PATH)/tools/perf $(BPF_LOAD_INCLUDE) \
	-I$(KERNEL_PATH)/tools/lib/ -I$(KERNEL_PATH)/tools/include \
	-c $(filter %.c,$^) -o  $@

$(OUTPUT)/%.o: %.c | $(OUTPUT)
	$(call msg,CC,$@)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(KERNEL_PATH)/samples/bpf -c $(filter %.c,$^) -o $@

$(APPS): %: $(OUTPUT)/%.o $(OUTPUT)/bpf_load.o | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ $(ALL_LDFLAGS) -I$(KERNEL_PATH)/samples/bpf -lelf -lbpf -lz -o $@

.DELETE_ON_ERROR:

.SECONDARY: