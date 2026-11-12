CC ?= cc
CLANG ?= clang
LLD_LINK ?= lld-link
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror

.PHONY: all test clean
all: pe64_probe fixture.exe

pe64_probe: pe64_probe.c
	$(CC) $(CFLAGS) $< -o $@

fixture.obj: fixture.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

fixture.exe: fixture.obj
	$(LLD_LINK) /machine:x64 /entry:win_entry /subsystem:console /nodefaultlib /dynamicbase /nxcompat /out:$@ $<

test: all
	python3 test.py

clean:
	rm -f pe64_probe fixture.obj fixture.exe
