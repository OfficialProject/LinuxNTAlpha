CC ?= cc
CLANG ?= clang
LLD_LINK ?= lld-link
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror

.PHONY: all test clean
all: pe64_probe fixture.exe import_fixture.exe

pe64_probe: pe64_probe.c
	$(CC) $(CFLAGS) $< -o $@

fixture.obj: fixture.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

fixture.exe: fixture.obj
	$(LLD_LINK) /machine:x64 /entry:win_entry /subsystem:console /nodefaultlib /dynamicbase /nxcompat /out:$@ $<

probe_dll.obj: probe_dll.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

probe.lib: probe_dll.obj
	$(LLD_LINK) /dll /noentry /machine:x64 /nodefaultlib /out:probe.dll /implib:$@ $<

import_fixture.obj: import_fixture.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

import_fixture.exe: import_fixture.obj probe.lib
	$(LLD_LINK) /machine:x64 /entry:win_entry /subsystem:console /nodefaultlib /dynamicbase /nxcompat /out:$@ import_fixture.obj probe.lib

test: all
	python3 test.py

clean:
	rm -f pe64_probe fixture.obj fixture.exe probe_dll.obj probe.dll probe.lib probe.exp import_fixture.obj import_fixture.exe
