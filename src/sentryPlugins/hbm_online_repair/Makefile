CC = gcc

CFLAGS += -Wall -Wtrampolines -O2 -g -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE

LDFLAGS += -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -pie -ltraceevent

SRC = $(wildcard *.c)
HDR = $(wildcard *.h)

OBJ = $(SRC:.c=.o)

TARGET = hbm_online_repair

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
