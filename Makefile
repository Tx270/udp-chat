CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
TARGET = build/udp-chat

SOURCES = $(shell find src -type f -name '*.c')
HEADERS = $(shell find src -type f -name '*.h')

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -Isrc $(SOURCES) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean