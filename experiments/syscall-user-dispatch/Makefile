CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror
TARGET := syscall_user_dispatch_probe

.PHONY: all test clean
all: $(TARGET)

$(TARGET): syscall_user_dispatch_probe.c
	$(CC) $(CFLAGS) $< -o $@

test: $(TARGET)
	@./$(TARGET); result=$$?; \
	if [ "$$result" -eq 77 ]; then echo 'SKIP: syscall user dispatch unsupported/blocked'; exit 0; fi; \
	exit "$$result"

clean:
	rm -f $(TARGET)
