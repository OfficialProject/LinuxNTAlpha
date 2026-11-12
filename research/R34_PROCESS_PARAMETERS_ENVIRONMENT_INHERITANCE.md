# R34 — Process parameters, command lines, environment blocks and inheritance

Research date: 2026-11-12. **Documentation review only; no Windows reference experiment or Linux runtime implementation performed.** This extends R12 (process lifecycle), R14 (security) and R32 (loader resolution) rather than claiming those topics are solved.

## Verified from Microsoft primary documentation

1. **Executable selection and command line are separate inputs.** `CreateProcessW` accepts an optional `lpApplicationName` and a separately optional, mutable `lpCommandLine`. When both are supplied, the application name selects the executable and the command-line buffer is passed to the child; the two strings need not be identical. The Unicode API may modify the command-line buffer. Its documented command-line limit is 32,767 UTF-16 characters including the terminator; if the application name is NULL, the module-name portion of the command line is limited to `MAX_PATH`. Source: [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
2. **Environment is a block, not a POSIX argv/envp equivalence.** `lpEnvironment == NULL` inherits the calling process environment; a supplied block consists of terminated `name=value` strings followed by an extra terminator. Unicode input requires `CREATE_UNICODE_ENVIRONMENT`. The API documents special per-drive current-directory entries such as `=C:` and says custom environment blocks must explicitly preserve these when needed and sort them. Source: [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
3. **Working directory is independently selectable.** `lpCurrentDirectory == NULL` inherits the calling process's current drive/directory; an explicit path may be a UNC path. This is distinct from both the executable location and command line. Source: [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
4. **Handle inheritance is object semantics, not just descriptor copying.** Microsoft documents inheritable handles, `bInheritHandles`, and an explicit allowlist via `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`. An inherited handle retains the same numeric value and access rights and refers to the same underlying object. `STARTF_USESTDHANDLES` governs standard-handle startup fields, whose values the caller must validate. Sources: [Inheritance](https://learn.microsoft.com/en-us/windows/win32/procthread/inheritance), [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
5. **Documented internal process-parameter structure is deliberately incomplete.** Microsoft's public `RTL_USER_PROCESS_PARAMETERS` declaration exposes `ImagePathName` and `CommandLine` with reserved fields and explicitly warns the structure may change. Do **not** infer a complete, stable Windows PEB/process-parameter layout from that declaration. Source: [RTL_USER_PROCESS_PARAMETERS](https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-rtl_user_process_parameters).

## Architectural implications (reasoned hypotheses, not measured)

- **HYPOTHESIS:** Model process creation as a transaction that resolves executable identity, Windows command-line text, environment, drive-specific working directories, startup handles and security context *before* publishing a runnable child. A plain Linux `execve(path, argv, envp)` bridge cannot by itself preserve all the documented Windows distinctions.
- **HYPOTHESIS:** Maintain a compatibility-layer handle namespace with explicit inheritance metadata and object identity, rather than assuming Linux file descriptor numbers satisfy Windows handle-value and access-right guarantees.
- **HYPOTHESIS:** Keep command-line text available verbatim to Windows API consumers; argument tokenization for a particular CRT is a separate concern. Do not silently substitute shell parsing or POSIX argv splitting.
- **OPEN:** Determine version- and architecture-specific TEB/PEB/process-parameter layouts using permitted Windows SDK interfaces and Windows reference probes; avoid hardcoding undocumented offsets as universal facts.
- **OPEN:** Define atomic failure/cleanup behavior when partial image mapping, handle duplication, environment setup, or thread creation fails. No correctness or latency claims are supported yet.

## Proposed conformance matrix (NOT executed)

| Case | Windows reference observation | LinuxNTAlpha acceptance target |
| --- | --- | --- |
| Distinct application name and command line | Child reports exact GetCommandLineW and image path | Both identities match reference |
| Quoted path, spaces, backslashes, Unicode | Capture raw command line and selected executable | No host-shell rewriting |
| Explicit Unicode environment, empty and inherited variants | Enumerate child environment and per-drive entries | Block contents and inheritance match |
| Explicit vs inherited current directory; UNC case | GetCurrentDirectoryW and drive-relative resolution | Consistent path semantics |
| Inheritable handles vs noninheritable; explicit handle list | Child checks numeric handles, object identity, rights | No accidental inheritance or right escalation |
| Invalid standard handles and startup flags | Record documented Windows outcome by OS build | Error behavior matched without assuming POSIX fd equivalence |
| Failure during process setup | Record externally visible status and leaked resources | Rollback and lifetime invariants hold |

Record Windows build, bitness, API variant, parent/child token context, and the exact test inputs; report PASS/FAIL/SKIP separately. This matrix is a plan, **not evidence of completed tests**.

## Priority and boundaries

Prioritize a small Windows-reference harness for command-line/environment/inheritance behavior before expanding the PE loader into process creation. R19–R28 only establish narrowly scoped PE execution and one independently implemented NTDLL export; they do not validate `CreateProcessW`, `GetCommandLineW`, `GetEnvironmentStringsW`, inherited NT handles, or full PEB initialization. No code has been imported from Wine, ReactOS, or Windows.
