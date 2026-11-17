# R142 — Windows NUMA topology and preferred-node allocation versus Linux NUMA policy

Research date: 2026-11-17. Status: **documentation analysis; no experiment**.

## Scope and evidence

**VERIFIED (documented Windows interface):** Windows exposes NUMA topology queries and NUMA-directed allocation interfaces, including `GetNumaHighestNodeNumber`, `GetNumaProcessorNodeEx`, `GetNumaNodeProcessorMaskEx`, `VirtualAllocExNuma`, and `CreateFileMappingNuma`. Microsoft's NUMA allocation example uses `QueryWorkingSetEx` to inspect where committed pages reside. A request for a node and observed placement are therefore separate things.

**VERIFIED (documented Linux interface):** Linux distinguishes per-task default memory policy (`set_mempolicy`), VMA/range policy (`mbind`), and shared-object policy. A per-task policy applies only to the installing thread and subsequently created threads; existing siblings retain their policies. VMA policy normally applies to future page allocations, not already faulted pages; migration is separately controlled. Cpuset memory-node restrictions take precedence. A VMA policy for a regular file-backed `MAP_SHARED` mapping does not control its page-cache placement as it does anonymous mappings.

**VERIFIED (documented Linux behavior):** `MPOL_PREFERRED` is a preference with fallback, whereas `MPOL_BIND` constrains allocation to eligible nodes. They are not interchangeable implementation choices. The kernel's policy documentation distinguishes the scope and fallback of each mode.

Primary sources (accessed 2026-11-17):
- Microsoft, NUMA support: https://learn.microsoft.com/en-us/windows/win32/procthread/numa-support
- Microsoft, Allocating Memory from a NUMA Node: https://learn.microsoft.com/en-us/windows/win32/memory/allocating-memory-from-a-numa-node
- Linux kernel, NUMA Memory Policy: https://docs.kernel.org/7.0/admin-guide/mm/numa_memory_policy.html
- Linux kernel, NUMA Memory Policy (rolling documentation): https://www.kernel.org/doc/html/latest/admin-guide/mm/numa_memory_policy.html

The Linux rolling documentation and versioned 7.0 documentation must not be treated as evidence that every feature is present in an older deployed kernel.

## Compatibility boundary

**HYPOTHESIS:** An NT-compatible runtime can represent guest NUMA node identities independently of Linux node IDs and translate guest preferred-node allocation requests into Linux range policy, subject to capability and permission checks. This may preserve useful locality without kernel modifications for a subset of private anonymous mappings.

**OPEN:** Whether Windows preference fallback, page commitment timing, mapping-object behavior, and working-set node reporting can be matched by a direct `mbind(MPOL_PREFERRED)` translation. The cited contracts alone do not establish exact equivalence.

**OPEN:** A Windows process can query a topology whose processor groups, node IDs and available-memory readings do not necessarily match the host's cpuset-visible topology. Returning raw host IDs may expose nodes that guest threads cannot run on or allocate from. This is a design hazard, not an experimentally demonstrated bug.

**OPEN:** File mappings, shared sections, copy-on-write views, child-process inheritance, migration, large pages, and concurrent policy changes require separate handling; a single process-wide `set_mempolicy` call cannot prove their compatibility.

## Proposed falsifiable gates (NOT RUN)

1. **Windows reference:** On a NUMA machine, record topology, allowed processor groups, requested node, successful/failed `VirtualAllocExNuma` allocations, first-touch behavior, and `QueryWorkingSetEx` observed placement. Include invalid nodes, memory pressure, and multiple threads. Record OS build and hardware topology.
2. **Linux capability:** On the same class of machine, record kernel version, NUMA nodes, `cpuset.mems.effective`, `set_mempolicy`/`mbind` availability and return codes, and actual page location via supported Linux interfaces. Distinguish successful policy installation from actual physical placement.
3. **Isolation:** Repeat with constrained cpusets and different thread policies; check that a guest's allocation request cannot bypass host memory-node restrictions.
4. **Section aliasing:** Compare shared versus private mappings, copy-on-write faults, and two processes mapping the same object. Never generalize private-anonymous success to shared page-cache semantics.
5. **Performance:** Measure allocation/fault and locality effects only after correctness, with node distances, sample counts, variability, and equivalent Windows/Linux baselines.

## Architectural consequence

**PROVISIONAL:** Keep guest NUMA topology and placement policy in a dedicated compatibility layer. Do not choose a kernel extension or claim a locality/performance gain until Windows reference tests and Linux host-policy tests pass. This topic complements R103 (CPU sets), R105 (section commitment), and R118 (large pages), without replacing their distinct gates.
