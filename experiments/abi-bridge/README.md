# x86-64 Windows/System V ABI bridge smoke test

Use **make test** on an x86-64 Linux system with GCC/compatible compiler.

This is a native Linux ELF program that calls GCC-generated Microsoft x64 ABI functions from System V callers and vice versa. It exercises zero arguments, eight integer arguments (crossing the four-register threshold), mixed integer/FP arguments, and a typed Microsoft-ABI function pointer. It prints PASS only when all expectations match.

The fixture passed on Linux 6.18.57, Debian 13, GCC 14.2.0 at -O0, -O2 and -O3 (2026-11-12). This proves a compiler-managed ABI crossing in one environment. It does **not** prove that arbitrary Windows PE functions can be called, that exception unwinding or TLS works, that XMM6–XMM15 save/restore is complete, or that variadic and aggregate calls are correct.

Sources:
- https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html
- https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- https://learn.microsoft.com/en-us/cpp/build/x64-software-conventions

Next: hand-assembled ABI fixtures for stack alignment/shadow space and nonvolatile vector registers; PE-compiled caller binary; Windows and Linux golden results; unwind across the boundary.
