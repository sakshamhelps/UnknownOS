# exe2unk - Makefile
# Author: byrex <byrex.me@proton.me>

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
LDFLAGS = -lz

TARGET = exe2unk
SRC = exe2unk.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install