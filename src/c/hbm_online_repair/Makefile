CC = gcc

CFLAGS = -Wall -o3

LDFLAGS = -ltraceevent

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
