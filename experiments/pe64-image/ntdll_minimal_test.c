#include "ntdll_minimal.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    const unsigned char a[] = {1,2,3,4,5,6,7,8};
    const unsigned char b[] = {1,2,3,10,5,6,7,8};
    const unsigned char c[] = {9,2,3,4,5,6,7,8};
    const unsigned char d[] = {1,2,3,4,5,6,7,10};
    size_t values[] = {
        lnta_RtlCompareMemory(a,b,8),
        lnta_RtlCompareMemory(a,a,8),
        lnta_RtlCompareMemory(a,b,0),
        lnta_RtlCompareMemory(a,c,8),
        lnta_RtlCompareMemory(a,d,8),
        lnta_RtlCompareMemory(a,b,3),
        lnta_RtlCompareMemory(a,a,1),
        lnta_RtlCompareMemory(NULL,NULL,0)
    };
    const size_t expected[] = {3,8,0,0,7,3,1,0};
    int errors = 0;
    for (size_t i=0; i<sizeof(values)/sizeof(values[0]); ++i) {
        if (values[i] != expected[i]) {
            fprintf(stderr, "case %zu: expected %zu got %zu\n", i, expected[i], values[i]);
            errors++;
        }
    }
    if (errors) return 1;
    puts("PASS: 8 RtlCompareMemory cases under MS x64 ABI");
    return 0;
}
