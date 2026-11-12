CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror
TARGET := sud_latency

.PHONY: all test benchmark clean
all: $(TARGET)

$(TARGET): sud_latency.c
	$(CC) $(CFLAGS) $< -o $@

test: $(TARGET)
	@./$(TARGET) 1000; result=$$?; \
	if [ "$$result" -eq 77 ]; then echo 'SKIP: Syscall User Dispatch disabled/unsupported'; exit 0; fi; \
	exit "$$result"

benchmark: $(TARGET)
	./$(TARGET) 20000

clean:
	rm -f $(TARGET)
