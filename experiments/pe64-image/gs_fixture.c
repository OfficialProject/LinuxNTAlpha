/* Deliberately synthetic PE x64 GS/TIB test. No real Windows TEB/NT support. */
#include <stdint.h>
#include <stddef.h>

__declspec(dllimport) int add_five(int);

struct fake_tib {
    uintptr_t fields[6];
    struct fake_tib *self;
    uint64_t cookie;
};

static volatile uint64_t number = 37;
static volatile uint64_t *volatile pn = &number;

__declspec(noinline) int win_entry(void) {
    uintptr_t t;
    __asm__ volatile("movq %%gs:0x30, %0" : "=r"(t));
    const struct fake_tib *tib = (const struct fake_tib *)t;
    if (tib->self != tib || tib->cookie != UINT64_C(0x1234abcd5678ef00))
        return -1;
    return add_five((int)*pn);
}
