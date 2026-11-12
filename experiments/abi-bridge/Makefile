CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror
TARGET := abi_bridge_probe

.PHONY: all test clean
all: $(TARGET)

$(TARGET): abi_bridge_probe.c
	$(CC) $(CFLAGS) $< -o $@

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
