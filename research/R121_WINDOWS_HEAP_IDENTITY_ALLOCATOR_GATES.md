# R121 — Windows heap identity, allocation lifetime and Linux allocator boundaries

Reviewed: 2026-11-16. **Published-contract research only; no implementation or experiment.**

## Scope and nonduplication
R11/R53/R105 concern virtual address reservation, sections and commitment; R95 concerns critical sections. This note addresses a different user-mode compatibility boundary: **Windows process/private heap handles, per-heap allocation ownership, reallocation failure, and heap policy**. A PE loader and basic ntdll export (R19/R28) do not establish Windows heap semantics.

## VERIFIED — Microsoft API contracts
1. [GetProcessHeap](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheap) obtains the default process heap; [HeapCreate](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapcreate) creates a private heap, and [GetProcessHeaps](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-getprocessheaps) enumerates active heaps. Heap identity is observable; a bare malloc/free wrapper does not supply these APIs.
2. [HeapAlloc](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc) allocates a nonmovable block associated with a specified heap. Its HEAP_ZERO_MEMORY and HEAP_GENERATE_EXCEPTIONS options affect observable behavior; default heap serialization and HEAP_NO_SERIALIZE constraints are documented. The API's alignment and error/exception behavior must be taken from the target Windows version, not inferred from a Linux allocator.
3. [HeapReAlloc](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heaprealloc) preserves existing contents on success and **does not free the original allocation on failure**. HEAP_REALLOC_IN_PLACE_ONLY and HEAP_ZERO_MEMORY have separate semantics. Blindly discarding the original pointer after host realloc failure is incorrect.
4. [HeapFree](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree) takes the owning heap handle; double-free and stale-pointer use can corrupt the heap. [HeapSize](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapsize), [HeapValidate](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapvalidate), and [HeapWalk](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapwalk) expose behavior beyond allocating bytes.
5. [HeapDestroy](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapdestroy) destroys a private heap and invalidates its handle; it is not a general substitute for destroying the process default heap. [HeapSetInformation](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapsetinformation) supports heap compatibility/resource policy and irreversible terminate-on-corruption policy; feature support is version-sensitive.

## HYPOTHESIS — proposed architecture, not implemented
- Model a guest process heap registry with distinct opaque guest heap handles and allocation-to-owner metadata. Use a host allocator internally **only as a byte-storage backend**, not as the guest heap identity or permission model.
- Ensure every guest-facing heap call has a deliberate error/exception and lifetime policy; do not expose raw host malloc metadata, libc arena pointers or allocator internals as guest handles.
- A shared backend allocator for multiple guest heaps may be feasible if it preserves isolation of ownership, enumeration, destruction and synchronization. This is not yet proven by Windows differential tests.
- Do not promise binary-identical heap layouts, allocation addresses, fragmentation patterns, LFH internals or segment-heap internals; investigate only when application-visible behavior or compatibility tests require them.

## OPEN — acceptance and security gates (tests NOT RUN)
1. **Windows reference:** on a recorded Windows build, create two private heaps and the default heap. Allocate/resize/free blocks; query sizes, enumerate heaps, test heap destruction, and record return values, last-error state and any raised exception. Include allocation failure, zero-size, HEAP_ZERO_MEMORY, HEAP_REALLOC_IN_PLACE_ONLY and HEAP_GENERATE_EXCEPTIONS cases with bounded sizes.
2. **Ownership:** attempt HeapFree/HeapReAlloc/HeapSize using a wrong but valid heap, and after heap destruction. Run risky invalid-pointer cases only in isolated child processes; record whether behavior is documented, observed or undefined. Do not turn one crash into a universal Windows guarantee.
3. **Concurrency:** test serialized default/private heaps with concurrent allocations; test HEAP_NO_SERIALIZE only with externally serialized access, never intentionally race a shared unsynchronized heap in the main test runner.
4. **Policy:** query HeapCompatibilityInformation and test HeapEnableTerminationOnCorruption in an isolated process; record OS version and whether a segment heap or other policy changes the observable response.
5. **Backend differential:** run the same safe fixture through the proposed guest heap service and compare return values, block contents, lifetime, heap enumeration, and error mapping. Test OOM with explicit resource limits; no claim of OOM equivalence from a host malloc NULL alone.
6. **Containment:** ensure guest pointers cannot address host allocator metadata, guest heap handles cannot be forged into host pointers, and guest raw syscalls cannot bypass any claimed sandbox boundary (R69/R90).

## Decision
Treat **heap services as a separate required user-mode subsystem**, not a side effect of implementing VirtualAlloc or forwarding to libc malloc. First implement observable heap identity/ownership and ordinary success/failure behavior; defer allocator-internal fidelity until differential evidence warrants it.

Primary sources: linked Microsoft API reference pages above, consulted 2026-11-16. No Windows fixture, Linux fixture, benchmark, code change or compatibility result is claimed.
