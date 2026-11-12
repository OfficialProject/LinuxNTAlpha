CC ?= cc
CFLAGS ?= -O2 -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror
TARGET := ntsync_capability_probe

.PHONY: all test clean
all: $(TARGET)

$(TARGET): ntsync_capability_probe.c
	$(CC) $(CFLAGS) $< -o $@

test: $(TARGET)
	@./$(TARGET); result=$$?; \
	if [ "$$result" -eq 77 ]; then echo 'SKIP: ntsync UAPI or device not available in this environment'; exit 0; fi; \
	exit "$$result"

clean:
	rm -f $(TARGET)
