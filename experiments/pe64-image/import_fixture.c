#include <stdint.h>
__declspec(dllimport) int add_five(int);
static volatile uint64_t value = 37;
static volatile uint64_t *volatile pv = &value;
__declspec(noinline) int win_entry(void) { return add_five((int)*pv); }
