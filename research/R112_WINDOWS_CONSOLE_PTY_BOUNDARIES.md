# R112 — Windows console objects versus Linux pseudoterminals

Status: **DOCUMENTATION REVIEW ONLY** (2026-11-15). No Windows differential test, Linux PTY probe, performance benchmark, or implementation is claimed.

## Scope and source-grounded findings

**VERIFIED (Microsoft API contract).** Windows standard handles are process table entries that may designate console input/screen buffers, files, or pipes; `GetStdHandle` can return redirected handles. The values are not automatically validated when set. `ReadConsole` and `WriteConsole` require console handles and fail for redirected non-console handles, whereas `ReadFile` and `WriteFile` can operate on redirected streams. `ReadConsoleInput` exposes input records that character-stream reads do not preserve. Console input and output modes, including line input, echo, and virtual-terminal processing, are separately configured.

**VERIFIED (Microsoft API contract).** Windows ConPTY, introduced with Windows 10 version 1809, exposes UTF-8 text and virtual-terminal sequences through input/output streams to a host terminal; it is not a declaration that every Windows console API is a byte-stream operation. The ConPTY owner must manage its pipe I/O and lifecycle; the optional inherited-cursor protocol requires asynchronous servicing to avoid a hang.

**VERIFIED (Linux documented interface).** Linux pseudoterminals are master/slave character-device pairs. The slave behaves as a terminal, and Linux terminal line discipline, session/controlling-terminal rules, termios modes and window-size handling are host semantics. `openpty` can allocate the pair; `login_tty` establishes a controlling terminal and standard descriptors.

**ARCHITECTURAL INFERENCE.** Mapping guest console handles directly to host PTY file descriptors cannot by itself implement Windows console screen-buffer identity, structured input records, mode-dependent Unicode APIs, or Windows handle-redirection behavior. A guest console-object layer with an optional PTY/VT transport is a candidate, not a selected architecture. Treating all output as UTF-8 would also conflate `WriteConsoleW` UTF-16 code units with ConPTY transport encoding.

## Proposed falsifiable gates (NOT RUN)

1. On a pinned Windows build, compare `GetStdHandle`, `GetConsoleMode`, `ReadConsoleW`, `WriteConsoleW`, `ReadFile` and `WriteFile` with genuine console handles, redirected anonymous pipes and files. Record error codes, byte/character counts and modes.
2. Exercise `ReadConsoleInputW` with keyboard, mouse and resize records; compare with character-stream reads. Capture ordering and which records are consumed or discarded.
3. Test Unicode supplementary characters and code-page changes across `WriteConsoleW`, `WriteConsoleA`, redirected `WriteFile`, and ConPTY UTF-8 output; record exact bytes/code units, not screenshots.
4. In a Windows ConPTY host, test pipe backpressure, resize, cursor-inheritance response, detach and shutdown with watchdog timeouts; distinguish API errors from hangs.
5. On Linux, run isolated PTY probes for termios canonical/raw mode, echo, window size, control-character signal delivery and controlling-terminal setup. These validate host facilities only, **not** Windows conformance.
6. Model process-local standard-handle replacement independently of console-object lifetime; test inheritance, duplicated handles and redirection without leaking host descriptors.

## Implementation and security questions (OPEN)

- Which Windows console API subset is needed for the first native PE acceptance target, and which can be explicitly unsupported?
- Where will screen-buffer state, input records, guest access rights and process attachment live?
- Can a Linux PTY be a backend without exposing host session signals or terminal control unexpectedly to guest code?
- How will VT escape handling, Unicode conversion and malformed input be bounded and fuzzed?

## Primary references

- Microsoft, [GetStdHandle](https://learn.microsoft.com/en-us/windows/console/getstdhandle)
- Microsoft, [ReadConsole](https://learn.microsoft.com/en-us/windows/console/readconsole)
- Microsoft, [WriteConsole](https://learn.microsoft.com/en-us/windows/console/writeconsole)
- Microsoft, [GetConsoleMode](https://learn.microsoft.com/en-us/windows/console/getconsolemode)
- Microsoft, [SetConsoleMode](https://learn.microsoft.com/en-us/windows/console/setconsolemode)
- Microsoft, [CreatePseudoConsole](https://learn.microsoft.com/en-us/windows/console/createpseudoconsole)
- Linux man-pages, [pty(7)](https://man7.org/linux/man-pages/man7/pty.7.html)
- Linux man-pages, [openpty(3)](https://man7.org/linux/man-pages/man3/openpty.3.html)

References checked 2026-11-15. This note does not establish behavior across all Windows versions or Linux kernel versions.
