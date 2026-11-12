#ifndef LINUXNTALPHA_NTDLL_MINIMAL_H
#define LINUXNTALPHA_NTDLL_MINIMAL_H
#include <stddef.h>
#if !defined(__x86_64__) || !defined(__GNUC__)
#error "This experiment targets x86-64 and GCC-compatible ms_abi"
#endif
size_t __attribute__((ms_abi)) lnta_RtlCompareMemory(const void *source1,
                                                      const void *source2,
                                                      size_t length);
#endif
