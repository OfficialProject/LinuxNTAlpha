# R212 — Winsock asynchronous name resolution, namespace routing and Linux backend gates

Status: **documentation review only (2026-11-21 UTC)**. No Windows-reference execution, Linux prototype, or performance measurement was performed.

## Architectural question

Can Linux `getaddrinfo(3)` or GNU `getaddrinfo_a(3)` directly implement the Windows `GetAddrInfoExW` contract, including provider selection, asynchronous completion, cancellation, timeout, and result ownership?

**Conclusion (derived from the primary contracts): no direct one-to-one mapping is justified.** A guest-visible resolver request and namespace-policy layer is required even if a host resolver supplies DNS answers. This is a proposed design constraint, not a tested implementation.

## Verified documentary findings

1. Microsoft's `GetAddrInfoExW` API selects a namespace through `dwNameSpace` and optionally a specific provider GUID through `lpNspId`. `NS_ALL` and `NS_DNS` are not interchangeable; the documented namespace set also includes NETBT, WINS, NLA, Bluetooth, and extensible third-party providers. The Linux system resolver is not a representation of the installed Windows Winsock namespace-provider catalog. [M1]
2. The Unicode `GetAddrInfoExW` interface supports an `OVERLAPPED` operation with completion callback or event and an optional cancellation handle. Microsoft documents Windows-version and ANSI/Unicode restrictions, including no asynchronous support on Windows 7 and earlier, and Windows 8 event/callback constraints. A compatibility layer must select a target Windows profile before asserting behavior. [M1]
3. On asynchronous callback completion, the `OVERLAPPED.Pointer` member is documented to refer to the original result-pointer argument; successful allocated `ADDRINFOEX` results must be freed by `FreeAddrInfoEx`. The callback's byte-count field is reserved. The guest allocator and result-list ownership therefore cannot simply expose a glibc `struct addrinfo` pointer. [M1]
4. Microsoft's `GetAddrInfoExCancel` documentation says cancellation invokes the caller's completion mechanism with `WSA_E_CANCELLED` and no results; an already-completed/timed-out or invalid operation handle returns `WSA_INVALID_HANDLE`. It explicitly warns that underlying synchronous providers may keep running after guest-visible cancellation. Thus **guest-visible cancellation and backend work termination are different events**. [M2]
5. GNU `getaddrinfo_a` offers asynchronous host lookups, but its `gai_cancel` cannot cancel a request already being processed, returning `EAI_NOTCANCELED`; `gai_error` and notification use GNU-specific status and notification mechanisms, not Windows `OVERLAPPED`/Winsock errors. [L1]
6. The Microsoft `SetAddrInfoExW` page states that its timeout, overlapped, completion routine, and name-handle parameters are reserved/unsupported; **do not infer symmetric asynchronous registration behavior from the Get API**. [M3]

## Proposed architecture (hypothesis, not implemented)

Maintain a guest resolver request with a stable identity/generation, target process and provider scope, guest output/result ownership, timeout deadline, terminal result, and exactly one guest-visible terminal notification. Backend DNS work may outlive guest cancellation; quarantine any late result, free host resources, and never publish a stale guest pointer. Serialize races among provider completion, explicit cancellation, timeout and process teardown. Keep the guest callback/event delivery mechanism distinct from Linux signal-based `getaddrinfo_a` notification; the existing NT APC/event research is relevant, but its semantics must be independently validated for this API.

A policy-controlled provider registry should distinguish supported `NS_DNS` from `NS_ALL`, unknown GUIDs, and other namespace types. Unsupported providers must produce a tested Windows-compatible failure rather than silently querying DNS. Host name-resolution configuration, search domains and network namespaces are potentially observable; the security boundary must prevent a guest from escaping its intended network context through a privileged host resolver. These are **design hypotheses and security requirements**, not established Windows behavior.

## Differential conformance matrix (not run)

- Synchronous `GetAddrInfoExW`: numeric IPv4/IPv6, canonical-name requests, service-name mapping, empty/null name, malformed hints, unknown flags and error mapping.
- Namespace selection: `NS_DNS` versus `NS_ALL`, unknown namespace, specific valid/invalid provider GUID, and installed-provider enumeration on a controlled Windows reference. Record OS version and provider inventory.
- Asynchronous callback versus manual-reset event: immediate completion, pending completion, `OVERLAPPED.Pointer`, `GetAddrInfoExOverlappedResult`, result allocation and `FreeAddrInfoEx`.
- Race schedule: cancel before backend dispatch, cancel while active, cancel after completion, timeout/cancel collision, repeated cancellation, request-handle reuse, and process teardown. Record exactly one guest terminal notification and whether late backend work is still running.
- Windows-version matrix: Windows 7 versus Windows 8+ where reference systems are available; distinguish Unicode from ANSI API behavior rather than guessing.
- Linux backend: compare synchronous `getaddrinfo` worker threads against `getaddrinfo_a`; instrument outstanding work after guest cancellation and check memory/resource cleanup. This is **not** a throughput benchmark until executed.

## Decision gates

Do **not** equate `gai_cancel` success with `GetAddrInfoExCancel` success, expose host `addrinfo` layouts to PE callers, assume DNS-only namespace behavior, or declare callback ordering without Windows-reference tests. The first practical prototype should use an injectable deterministic fake provider to force completion/cancellation races before live DNS is introduced. Any claims of exact Windows parity remain **OPEN** pending reference traces.

## Primary sources

- [M1] Microsoft, `GetAddrInfoExW`: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfoexw
- [M2] Microsoft, `GetAddrInfoExCancel`: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfoexcancel
- [M3] Microsoft, `SetAddrInfoExW`: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-setaddrinfoexw
- [L1] Linux man-pages project, `getaddrinfo_a(3)`: https://man7.org/linux/man-pages/man3/getaddrinfo_a.3.html

## Relation to existing research

Extends the Winsock request/cancellation themes in R195 and the transport-oriented R196/R209/R211, but covers **name-service namespace routing and resolver-request lifetime**, not socket I/O. No experiment has been added or claimed.
