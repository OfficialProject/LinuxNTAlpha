/* Trusted Windows-targeted PE64 fixture importing a documented ntdll routine. */
#include <stddef.h>
__declspec(dllimport) size_t RtlCompareMemory(const void *, const void *, size_t);
static const unsigned char input_a[8] = {1,2,3,4,5,6,7,8};
static const unsigned char input_b[8] = {1,2,3,10,5,6,7,8};
static const unsigned char *volatile ptr_a = input_a;
static const unsigned char *volatile ptr_b = input_b;
__declspec(noinline) int win_entry(void) {
    size_t match = RtlCompareMemory(ptr_a, ptr_b, 8);
    return match == 3 ? 42 : -1;
}
