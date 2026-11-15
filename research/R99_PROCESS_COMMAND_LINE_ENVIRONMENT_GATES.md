# R99 — Windows process command-line and environment fidelity versus Linux execve

Status: RESEARCH ONLY — no implementation or Windows differential experiment. Reviewed 2026-11-15.

## Question

Can an NT-compatible PE process bootstrap use Linux `execve(argv, envp)` as its sole representation of Windows process parameters? **No, not without preserving additional guest-visible state.** This is an interface mismatch, not proof that a Linux-hosted implementation is impossible.

## Verified primary-source contracts

1. **Windows preserves a process command-line string.** `GetCommandLineW` returns a system-managed pointer to the current process command-line string. Its lifetime is not caller-owned; the executable name exposed in the string may differ from the text originally supplied to `CreateProcess` because the system may qualify it. Microsoft: [GetCommandLineW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getcommandlinew).
2. **Argument parsing is a separate API, not an intrinsic process launch representation.** `CommandLineToArgvW` parses a Unicode command line; it documents quote/backslash parity rules, leading-whitespace behavior (empty first argument), and an empty-input special case returning the executable path. Its result is freed with `LocalFree`. Microsoft: [CommandLineToArgvW](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw). Do not assume that every CRT startup parser is identical to this Shell API.
3. **Windows process environment is a double-NUL-terminated sequence of name=value strings.** A child ordinarily inherits a copy, or the creator may supply a replacement block. Microsoft: [Environment Variables](https://learn.microsoft.com/en-us/windows/win32/procthread/environment-variables).
4. **Ordering is a documented requirement for constructed environment blocks.** Microsoft specifies case-insensitive, locale-independent Unicode-name sorting. Microsoft: [Changing Environment Variables](https://learn.microsoft.com/en-us/windows/win32/procthread/changing-environment-variables).
5. **Environment snapshots have ownership semantics.** `GetEnvironmentStringsW` returns a block to be treated as read-only and released using the matching `FreeEnvironmentStringsW` API. Mutation is via `SetEnvironmentVariable`, not by modifying the returned block. Microsoft: [GetEnvironmentStringsW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getenvironmentstringsw), [FreeEnvironmentStringsW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-freeenvironmentstringsw).
6. **Environment length limits cannot be guessed from Linux ARG_MAX.** Microsoft documents a 32,767-character maximum for a user-defined variable, no technical limit for the modern environment block itself, and practical limits imposed by individual launch/access mechanisms. Microsoft: [Environment Variables](https://learn.microsoft.com/en-us/windows/win32/procthread/environment-variables).

## Architectural implication (reasoned, not measured)

Linux process startup provides an argument vector and environment vector; Windows exposes a raw command-line string plus Windows-specific environment APIs. Converting Linux `argv` back into a command line cannot generally recover original quoting and whitespace. A proposed PE launcher should therefore accept and retain **raw UTF-16 command-line text**, **guest environment state**, and **guest executable identity** as separate fields, then expose the documented Windows API behavior independently of host `argv` and `envp`.

This is a proposed design constraint, **not** evidence of a correct PEB / RTL_USER_PROCESS_PARAMETERS layout. Internal structure layouts, pointer stability across mutation, drive-current-directory pseudo-variables, duplicate-name handling, and `CreateProcessW` quoting details remain OPEN and require separate primary-source review and Windows-reference measurements. No claim is made that a UTF-8 Linux environment can losslessly represent arbitrary Windows UTF-16 strings.

## Falsifiable Windows-reference gates (not executed)

- Create child processes with quoted paths, embedded spaces, backslashes before quotes, leading whitespace, and empty command lines; record parent-supplied text and child `GetCommandLineW` output separately.
- Compare `CommandLineToArgvW` results against the target CRT's own startup `argv` on the same inputs; record any differences rather than normalizing them away.
- Supply explicit Unicode environment blocks with mixed-case variable names and non-ASCII values; query child `GetEnvironmentStringsW`, `GetEnvironmentVariableW`, and inheritance after parent mutation.
- Test malformed blocks and boundary lengths in isolated child processes; distinguish documented behavior from observed implementation details.
- Capture Windows build, architecture, API variant, error codes, and reproducible fixture source before assigning any PASS/FAIL status.

## Integration priority

Add a process-parameter contract to the native PE launch design **before** treating Linux `execve` argument vectors as guest process state. This complements R89 (DOS device paths), R64 (DLL lifecycle), and R73 (process lifetime); it does not resolve those subjects.

**Evidence classification:** VERIFIED = cited Microsoft API contracts; HYPOTHESIS = suggested guest process-parameter model; OPEN = internal layouts and undocumented edge behavior. **Experiments executed: none.**
