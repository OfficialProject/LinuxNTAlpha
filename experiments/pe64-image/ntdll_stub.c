/* Linker fixture only: ntdll.dll is NEVER loaded by the Linux test. */
#include <stddef.h>
__declspec(dllexport) size_t RtlCompareMemory(const void *a, const void *b, size_t count) {
    const unsigned char *x = a, *y = b;
    size_t i = 0;
    while (i < count && x[i] == y[i]) ++i;
    return i;
}
