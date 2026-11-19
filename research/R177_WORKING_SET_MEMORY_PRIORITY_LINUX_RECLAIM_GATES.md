# R177 — Windows working-set policy and memory priority versus Linux reclaim

Status: **research only** (2026-11-19). No Windows reference run, Linux experiment, kernel patch, benchmark, or implementation is claimed.

## Scope and non-duplication

R170 covers large-page allocation, R171 virtual-address placeholders, R142 NUMA placement, and R76 job-object/cgroup boundaries. This note instead addresses **per-process resident working-set targets, hard working-set flags, and page-trimming priority**. These are neither virtual address reservation nor job commit accounting.

## Verified API contracts (primary sources)

1. **Windows working-set bounds are resident-page policy, not an allocation/commit ceiling.** `SetProcessWorkingSetSizeEx` accepts a process handle requiring `PROCESS_SET_QUOTA`, minimum and maximum resident working-set sizes, and enforcement flags. Its ordinary limits influence trimming and are not guaranteed bounds; the HARDWS_MIN/MAX flags request enforced bounds. A pair of `(SIZE_T)-1` values requests trimming as many resident pages as possible. Raising bounds may require `SeIncreaseWorkingSetPrivilege`. The corresponding getter exposes current configured bounds and flags. [Microsoft: SetProcessWorkingSetSizeEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-setprocessworkingsetsizeex); [Microsoft: GetProcessWorkingSetSizeEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-getprocessworkingsetsizeex).
2. **Memory priority is a reclaim hint, not CPU scheduling priority.** `SetProcessInformation(ProcessMemoryPriority)` accepts `MEMORY_PRIORITY_INFORMATION` (requiring `PROCESS_SET_INFORMATION`); documented priority levels are 1 through 5, with 5 the default. The process priority influences the default priority of pages added by its threads; lower-priority pages tend to be trimmed earlier when other factors are equal. Thread-level priority is separately exposed. [Microsoft: SetProcessInformation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation); [Microsoft: MEMORY_PRIORITY_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-memory_priority_information).
3. **Linux primitives have different scopes and promises.** `madvise` operates on page ranges and offers reclaim hints such as `MADV_COLD`/`MADV_PAGEOUT`; `MADV_DONTNEED` can change subsequent private-anonymous reads to zero-fill and is therefore not a safe generic translation for trimming arbitrary Windows pages. `mincore` reports a momentary residency snapshot, not a stable working-set ledger. `mlock` prevents swapping selected ranges and is subject to resource limits, rather than implementing a Windows process-wide hard minimum/maximum. [Linux madvise(2)](https://man7.org/linux/man-pages/man2/madvise.2.html); [mincore(2)](https://man7.org/linux/man-pages/man2/mincore.2.html); [mlock(2)](https://man7.org/linux/man-pages/man2/mlock.2.html).
4. **Linux cgroup v2 memory controls are hierarchical accounting/reclaim controls.** Their memory controller covers charges across a cgroup, potentially multiple processes and page-cache/shared-memory accounting; its `memory.min`, `memory.low`, `memory.high`, and `memory.max` interfaces are not documented as Windows per-process resident working-set limits. [Linux kernel: cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html).

## Architectural inference — HYPOTHESIS, not validated

Maintain separate guest process working-set configuration and memory-priority state, with access checks and accurate get/set round trips. Use Linux reclaim controls only as explicitly **best-effort backends**, with capability negotiation. Do **not** silently map Windows hard working-set minimum to `mlockall` (which may pin unrelated mappings, consume host quota, and alter fault behavior), hard maximum to `memory.max` (which can impose cgroup-wide pressure/OOM), or trim to `MADV_DONTNEED` (which can discard anonymous contents). Linux cannot be assumed to provide a direct exact implementation of Windows hard resident-set bounds without further kernel investigation.

A guest API success result must not be fabricated solely because metadata was stored: establish whether the requested policy can actually be enforced on the target host and Windows version. Until tested, classify hard-bound conformance as **OPEN**.

## Proposed differential conformance gates — NOT RUN

- **Access and validation:** insufficient process rights; contradictory hard flags; min > max; minimum/maximum boundary values; privilege-gated increases; query after failed and successful calls.
- **Soft versus hard:** touch distinct anonymous and mapped-file pages; compare configured bounds, measured working set, fault counts, and contents under controlled idle/pressure conditions; distinguish API success from actual enforcement.
- **Trim:** request both bounds as `(SIZE_T)-1`; verify private-anonymous contents survive; record resident-page changes and subsequent faults.
- **Priority:** process default and thread overrides, levels 1–5 and invalid values; observe reclaim under controlled competition without promising deterministic victim ordering.
- **Linux safety:** shared mappings, COW, page cache, THP, cgroup membership, `RLIMIT_MEMLOCK`, insufficient privilege, and multi-process cgroup interference. Reject any backend mapping that changes guest-visible bytes or kills unrelated processes.
- **Reproducibility:** pin Windows edition/build, Linux kernel/config, cgroup hierarchy, page size, RAM/swap, privilege/token, mapping types, repeated trials and confidence intervals. Do not infer Windows behavior from a Linux-only test.

## Unresolved questions

1. Which supported Windows builds enforce hard working-set limits under pressure, and what do the observable return/error contracts look like? Official API descriptions are not a substitute for a versioned reference run.
2. Is there a safe Linux kernel interface or narrowly scoped extension capable of per-process resident-set hard policy without disturbing shared mappings and host reclaim invariants?
3. What exact NT native information classes and status translations must the initial compatibility target expose? Do not assume Win32 wrapper behavior fully specifies the NT layer.
4. How should memory-priority page attribution interact with guest thread lifetime and host reclaim without imposing global performance regressions?

Sources reviewed 2026-11-19. This document reports documentation contracts and test designs only.
