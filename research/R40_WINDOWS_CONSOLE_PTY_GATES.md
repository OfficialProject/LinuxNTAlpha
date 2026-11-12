# R40 — Windows console input records versus Linux PTY streams

Date: 2026-11-12. Status: **PRIMARY-DOCUMENTATION REVIEW ONLY**. No Windows reference traces, Linux PTY experiment, or LinuxNTAlpha console implementation were run for this note.

## Scope

This is a new compatibility boundary distinct from R34 process standard-handle inheritance and R37 named-pipe semantics: **a Windows console input buffer is not merely a byte stream**. The project must decide whether a first milestone supports redirected stdin/stdout only, classic Win32 console APIs, or ConPTY-style terminal hosting. These are different acceptance claims.

## VERIFIED — published Microsoft contracts

1. `GetConsoleMode` / `SetConsoleMode` distinguish input-buffer modes from screen-buffer output modes. Input modes include processed input, line input, echo, mouse input, window input, and virtual-terminal input. The documented mouse and window input records are accessible through `ReadConsoleInput`, but not equivalently through `ReadFile`/`ReadConsole`. [GetConsoleMode](https://learn.microsoft.com/en-us/windows/console/getconsolemode), [SetConsoleMode](https://learn.microsoft.com/en-us/windows/console/setconsolemode).
2. `CreatePseudoConsole` takes dimensions, separate input/output stream handles, flags and returns an `HPCON`. The documented input/output handles are currently restricted to synchronous I/O. Plain text and virtual-terminal sequences are transported on the streams; the pseudoconsole handle has an explicit close lifecycle. [CreatePseudoConsole](https://learn.microsoft.com/en-us/windows/console/createpseudoconsole).
3. `ResizePseudoConsole` updates pseudoconsole dimensions so attached console applications querying console functions can see the new dimensions; it is not simply a terminal-display cosmetic operation. [ResizePseudoConsole](https://learn.microsoft.com/en-us/windows/console/resizepseudoconsole).
4. Microsoft documents a distinct `ENABLE_VIRTUAL_TERMINAL_INPUT` mode that translates console-window input into VT sequences for applications using `ReadFile` or `ReadConsole`. That mode does not erase the separate classic `ReadConsoleInput` event-record interface. [SetConsoleMode](https://learn.microsoft.com/en-us/windows/console/setconsolemode).

These are documented API contracts, **not observed behavior** on a particular Windows build. Primary pages consulted 2026-11-12. No source code was copied.

## Architectural inference — NOT VERIFIED by experiment

A Linux PTY is a plausible *transport and interactive terminal backend* for a subset of Windows console use cases. It cannot, by itself, prove the Win32 input-event record model, screen-buffer APIs, control-event behavior, handle identity, console attachment or Windows error behavior. A bridge may need a console session service holding explicit input-record queues, per-buffer modes, screen state, process attachments and lifetime rules. This is a proposed model, not an approved architecture.

In particular, mapping all guest console handles directly to Linux file descriptors risks losing the distinction between console input buffers, screen buffers, redirected pipes and pseudoconsole sessions. R34's inherited standard handles are necessary but insufficient.

## Proposed falsifiable conformance gates — NOT EXECUTED

1. **Handle classification:** On a pinned Windows build, compare console-attached stdin, redirected stdin, a pipe, and a file under `GetConsoleMode`, `GetFileType`, `ReadFile`, `ReadConsole` and `ReadConsoleInput`. Record success/failure, last error and bytes/events consumed. Do not assume all standard handles refer to a console.
2. **Input mode matrix:** Toggle processed, line, echo, VT, mouse and window modes; inject keyboard and resize events using an actual Windows console reference environment. Record which APIs receive character data versus `INPUT_RECORD` entries and whether toggling modes changes queue behavior.
3. **Output behavior:** Compare screen-buffer dimensions, cursor position, text attributes, scroll behavior and VT processing between native console APIs and a ConPTY-attached process. Do not infer that rendering ANSI sequences reproduces classic console state.
4. **Lifecycle:** Test console attach/detach, child inheritance, closure of parent/child handles, resize after close, blocked reads during shutdown and control-event handling. Pin OS build and distinguish documented guarantees from observed edge cases.
5. **Transport prototype:** Only after recording the Windows oracle, compare a Linux PTY-backed bridge against the same application-level observations; include UTF-8 split sequences, wide characters, input bursts, EOF, resize and backpressure. Treat mismatches as compatibility failures, not as performance regressions.
6. **Security:** Bound queue sizes and dimensions; separate untrusted VT/control-sequence parsing from host terminal privileges. The existing PE fixture runner is not a sandbox.

## OPEN

- Which minimum console API subset is needed for the first *real* Windows command-line program, and which programs require a full event-record console?
- How should the service model process-group control events and console attachment without conflating them with Linux process groups and POSIX signals?
- Can ConPTY-oriented VT behavior be layered over the same logical console state as classic Win32 APIs without semantic contradictions?
- Which APIs should explicitly return unsupported rather than silently pretending a redirected pipe is a console?

## Decision gate

**Do not label Linux PTY I/O as Windows console compatibility.** First establish a Windows-reference conformance oracle, then separately demonstrate redirected standard-stream support, classic console event handling, and pseudoconsole session behavior. No implementation or performance claim follows from this research.
