CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror
TARGET := gs_teb_probe

.PHONY: all test clean
all: $(TARGET)

$(TARGET): gs_teb_probe.c
	$(CC) $(CFLAGS) -pthread $< -o $@

test: $(TARGET)
	@./$(TARGET); result=$$?; \
	if [ "$$result" -eq 77 ]; then echo 'SKIP: ARCH_SET_GS or GS base access unsupported'; exit 0; fi; \
	exit "$$result"

clean:
	rm -f $(TARGET)
