CC ?= cc
CLANG ?= clang
LLD_LINK ?= lld-link
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror

.PHONY: all test clean
all: pe64_probe fixture.exe import_fixture.exe gs_fixture.exe sud_fixture.exe ntdll_fixture.exe ntdll_minimal_test

pe64_probe: pe64_probe.c ntdll_minimal.c ntdll_minimal.h
	$(CC) $(CFLAGS) pe64_probe.c ntdll_minimal.c -o $@

ntdll_minimal_test: ntdll_minimal_test.c ntdll_minimal.c ntdll_minimal.h
	$(CC) $(CFLAGS) ntdll_minimal_test.c ntdll_minimal.c -o $@

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

gs_fixture.obj: gs_fixture.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

gs_fixture.exe: gs_fixture.obj probe.lib
	$(LLD_LINK) /machine:x64 /entry:win_entry /subsystem:console /nodefaultlib /dynamicbase /nxcompat /out:$@ gs_fixture.obj probe.lib

sud_fixture.obj: sud_fixture.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

sud_fixture.exe: sud_fixture.obj
	$(LLD_LINK) /machine:x64 /entry:win_entry /subsystem:console /nodefaultlib /dynamicbase /nxcompat /out:$@ $<

ntdll_stub.obj: ntdll_stub.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -O2 -c $< -o $@

ntdll.lib: ntdll_stub.obj
	$(LLD_LINK) /dll /noentry /machine:x64 /nodefaultlib /out:ntdll.dll /implib:$@ $<

ntdll_fixture.obj: ntdll_fixture.c
	$(CLANG) -target x86_64-pc-windows-msvc -ffreestanding -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -O2 -c $< -o $@

ntdll_fixture.exe: ntdll_fixture.obj ntdll.lib
	$(LLD_LINK) /machine:x64 /entry:win_entry /subsystem:console /nodefaultlib /dynamicbase /nxcompat /out:$@ ntdll_fixture.obj ntdll.lib

test: all
	./ntdll_minimal_test
	python3 test.py

clean:
	rm -f pe64_probe fixture.obj fixture.exe probe_dll.obj probe.dll probe.lib probe.exp import_fixture.obj import_fixture.exe gs_fixture.obj gs_fixture.exe sud_fixture.obj sud_fixture.exe ntdll_stub.obj ntdll.dll ntdll.lib ntdll.exp ntdll_fixture.obj ntdll_fixture.exe ntdll_minimal_test
