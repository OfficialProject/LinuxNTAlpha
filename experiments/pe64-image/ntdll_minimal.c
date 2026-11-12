#include "ntdll_minimal.h"
/* Independently implemented public RtlCompareMemory prefix comparison contract.
 * Trusted-buffer test only: invalid/unmapped pointers are NOT guarded.
 */
size_t __attribute__((ms_abi, noinline)) lnta_RtlCompareMemory(const void *source1,
                                                                const void *source2,
                                                                size_t length) {
    const unsigned char *left = source1, *right = source2;
    size_t prefix = 0;
    while (prefix < length && left[prefix] == right[prefix]) ++prefix;
    return prefix;
}
