# 定义编译器
CC = clang

# 定义内核态编译选项
KERNEL_CFLAGS = -O2 -g -target bpf -D__TARGET_ARCH_arm64

# 定义用户态编译选项
USER_CFLAGS = -g -lbpf

# 定义目标文件
KERNEL_TARGET = ebpf_collector.bpf.o
USER_TARGET = ebpf_collector
SKELETON_HEADER = ebpf_collector.skel.h
VMLINUX_HEADER = vmlinux.h

# 默认目标
all: $(KERNEL_TARGET) $(VMLINUX_HEADER) $(SKELETON_HEADER) $(USER_TARGET)

# 生成 vmlinux.h 文件
$(VMLINUX_HEADER):
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > $(VMLINUX_HEADER)

# 编译内核态文件
$(KERNEL_TARGET): ebpf_collector.bpf.c $(VMLINUX_HEADER)
	$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# 生成骨架文件
$(SKELETON_HEADER): $(KERNEL_TARGET)
	bpftool gen skeleton $< > $@

# 编译用户态文件
$(USER_TARGET): ebpf_collector.c $(SKELETON_HEADER) $(VMLINUX_HEADER)
	$(CC) $(USER_CFLAGS) $< -o $@

# 清理生成的文件
clean:
	rm -f $(KERNEL_TARGET) $(VMLINUX_HEADER) $(SKELETON_HEADER) $(USER_TARGET)

# 伪目标，避免与文件名冲突
.PHONY: all clean
