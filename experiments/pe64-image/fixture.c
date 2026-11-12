#include <stdint.h>
static volatile uint64_t counter = 37;
static volatile uint64_t * volatile pointer_to_counter = &counter;
__declspec(noinline) int win_entry(void) {
    return (int)(*pointer_to_counter + 5);
}
