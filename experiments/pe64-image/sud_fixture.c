/* Windows-target PE64 fixture with an NT-*style* raw x86-64 syscall instruction.
 * Number 39 intentionally collides with Linux getpid. This is not a genuine
 * or stable Windows NT service number; no Windows service is implemented.
 */
#include <stdint.h>

static volatile uint64_t data = 37;
static volatile uint64_t *volatile pdata = &data;

__declspec(noinline) int win_entry(void) {
    long result;
    __asm__ volatile("movq %%rcx, %%r10\n\tmovl $39, %%eax\n\tsyscall"
                     : "=a"(result) : : "rcx", "r10", "r11", "memory");
    return result == 0x12345L && *pdata == 37 ? 42 : -1;
}
