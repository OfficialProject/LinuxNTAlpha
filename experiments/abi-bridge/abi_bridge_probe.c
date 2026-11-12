#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(__x86_64__) || !defined(__GNUC__)
#error "This probe requires x86-64 and GCC-compatible ABI attributes"
#endif

#define MSABI __attribute__((ms_abi, noinline))
#define SYSVABI __attribute__((sysv_abi, noinline))

static uint64_t hash8(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                      uint64_t e, uint64_t f, uint64_t g, uint64_t h) {
    return a*3 + b*5 + c*7 + d*11 + e*13 + f*17 + g*19 + h*23;
}

static MSABI uint64_t windows_zero(void) { return UINT64_C(0xabcddcba); }
static MSABI uint64_t windows_eight(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                                   uint64_t e, uint64_t f, uint64_t g, uint64_t h) {
    return hash8(a,b,c,d,e,f,g,h);
}
static SYSVABI uint64_t linux_eight(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                                   uint64_t e, uint64_t f, uint64_t g, uint64_t h) {
    return hash8(a,b,c,d,e,f,g,h);
}
static MSABI uint64_t windows_calls_linux(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                                           uint64_t e, uint64_t f, uint64_t g, uint64_t h) {
    return linux_eight(a,b,c,d,e,f,g,h);
}
static MSABI double windows_mixed(long a, double b, long c, float d, long e, double f) {
    return a * 1.25 + b * 2.0 + c * 0.5 + d * 4.0 + e + f * 3.0;
}
static SYSVABI double linux_mixed(long a, double b, long c, float d, long e, double f) {
    return a * 1.25 + b * 2.0 + c * 0.5 + d * 4.0 + e + f * 3.0;
}
static MSABI double windows_calls_linux_mixed(long a, double b, long c, float d, long e, double f) {
    return linux_mixed(a,b,c,d,e,f);
}

typedef uint64_t ( __attribute__((ms_abi)) *windows_eight_fn)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);

int main(void) {
    volatile uint64_t seed = 5;
    uint64_t a=seed, b=a+1, c=b+1, d=c+1, e=d+1, f=e+1, g=f+1, h=g+1;
    uint64_t expected = hash8(a,b,c,d,e,f,g,h);
    windows_eight_fn fn = windows_eight;
    uint64_t windows_result = fn(a,b,c,d,e,f,g,h);
    uint64_t reverse_result = windows_calls_linux(a,b,c,d,e,f,g,h);
    uint64_t zero_result = windows_zero();
    double expected_mixed = 3 * 1.25 + 2.5 * 2.0 + 5 * 0.5 + 1.25 * 4.0 + 7 + 0.75 * 3.0;
    double result_mixed = windows_mixed(3,2.5,5,1.25f,7,0.75);
    double reverse_mixed = windows_calls_linux_mixed(3,2.5,5,1.25f,7,0.75);
    int pass = windows_result == expected && reverse_result == expected && zero_result == UINT64_C(0xabcddcba) &&
        result_mixed == expected_mixed && reverse_mixed == expected_mixed;
    printf("%s expected=%" PRIu64 " windows=%" PRIu64 " reverse=%" PRIu64 " zero=%" PRIu64
           " mixed=%.2f reverse_mixed=%.2f\n", pass ? "PASS" : "FAIL", expected, windows_result,
           reverse_result, zero_result, result_mixed, reverse_mixed);
    return pass ? 0 : 1;
}
