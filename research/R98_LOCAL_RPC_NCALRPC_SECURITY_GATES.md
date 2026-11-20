# R98 — Local RPC (ncalrpc) identity, binding and Linux transport boundaries

Status (2026-11-20): **VERIFIED FROM PRIMARY DOCUMENTATION** for the cited Microsoft RPC contracts and Linux man-pages interface; **HYPOTHESIS** for a Linux-hosted compatibility design; **OPEN** for Windows-reference behavior, ALPC internals and implementation. **No experiments executed.**

## Why this is a separate gate

R37/R78 examine named pipes; R81/R92 examine impersonation and guest authorization; R89 examines object paths. None establishes the RPC *binding-level* identity and security contract. A local RPC endpoint is not merely a named pipe, Unix socket or arbitrary ALPC message queue. This note addresses documented RPC interfaces rather than claiming a complete or publicly specified ALPC kernel ABI.

## Verified contracts and source boundaries

1. **Local protocol and endpoint binding.** Microsoft's RPC string-binding documentation identifies `ncalrpc` as a protocol sequence and describes explicit endpoints versus endpoint-mapper resolution. A binding may omit an endpoint when the server registered with the endpoint mapper. Therefore guest endpoint discovery and binding lifetime are separate from Unix socket pathname lookup. [Microsoft: String Binding](https://learn.microsoft.com/en-us/windows/win32/rpc/string-binding).
2. **Default local security is not mutual server authentication.** The Microsoft `RPC_BINDING_HANDLE_SECURITY_V1` contract says `ncalrpc` uses Windows-kernel transport-level security, with default privacy, impersonation, and *no mutual authentication* when its security structure is omitted. It also distinguishes **static** identity tracking for `RpcBindingCreate` from **dynamic** tracking for `RpcBindingFromStringBinding` without explicit settings. An implementation that captures one immutable Unix UID at connect time cannot automatically reproduce both modes. [Microsoft: RPC_BINDING_HANDLE_SECURITY_V1_A](https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/ns-rpcdce-rpc_binding_handle_security_v1_a).
3. **Authentication-level labeling is transport-specific.** Microsoft's authentication-level constants state that `ncalrpc` always uses `RPC_C_AUTHN_LEVEL_PKT_PRIVACY`, regardless of the requested constant. This is a Windows RPC API observation, not a claim that local Linux socket payloads are encrypted. [Microsoft: Authentication-Level Constants](https://learn.microsoft.com/en-us/windows/win32/rpc/authentication-level-constants).
4. **Server endpoint authorization is distinct from per-call policy.** Microsoft documents an optional endpoint security descriptor for `RpcServerUseProtseq` with `ncalrpc` and `ncacn_np`; absent a supplied descriptor, a token-derived default descriptor applies. Microsoft separately recommends checking authentication service, level and client identity on calls. Do not equate connection permission with authorization for every RPC method. [Microsoft: Using Transport-level Security on the Server](https://learn.microsoft.com/en-us/windows/win32/rpc/using-transport-level-security-on-the-server); [Microsoft: Mutual Authentication in RPC Applications](https://learn.microsoft.com/en-us/windows/win32/ad/mutual-authentication-in-rpc-applications).
5. **Guest impersonation is not a host credential switch.** RPC quality-of-service documentation distinguishes identify, impersonate and delegate levels, plus identity tracking. Linux `SO_PEERCRED`/`SCM_CREDENTIALS` supply Linux PID/UID/GID, while `SCM_RIGHTS` passes host open-file-description references. None encodes NT tokens, SIDs, guest impersonation levels or NT handle rights. [Microsoft: RPC_SECURITY_QOS](https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/ns-rpcdce-rpc_security_qos); [Linux man-pages: unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html).
6. **Endpoint permissions are not a universal defense.** Linux abstract Unix sockets have no meaningful socket-file permissions; pathname sockets use host filesystem permissions. Thus neither mode alone implements the guest endpoint DACL, mutual server identity checks or RPC method authorization. [Linux man-pages: unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html).

## Architectural hypothesis — not implemented

Implement a **guest RPC binding authority** above a private host transport. Keep separate records for endpoint registration and lookup, guest server principal, guest client effective token at the documented tracking point, binding QoS, interface registration, method authorization, call identity and cancellation. Host Unix sockets could transport framed messages, but their credentials would only authorize access to the *host broker*; the broker must independently enforce guest security. Treat Linux FD passing as a privileged, explicitly mediated operation rather than as NT handle transfer.

**Unresolved:** exact ALPC message/attribute layout, server impersonation timing, endpoint-mapper collision behavior, cancellation/rundown ordering, RPC context-handle lifetime, per-call identity changes, and any performance advantage. The cited public RPC APIs do not prove a particular ALPC implementation. No Windows kernel behavior has been reverse engineered here.

## Falsifiable reference and backend tests — proposed, not run

- On a recorded Windows build, create a minimal documented `ncalrpc` RPC client/server and log explicit versus mapper-resolved endpoints, binding status and server invocation. Include same-name collision and server teardown/re-registration.
- Compare `RpcBindingCreate` and `RpcBindingFromStringBinding` under thread-token changes, with and without explicit identity tracking. Record client/server identities, API return codes and whether each call reaches the server; never assume results from documentation alone.
- Compare default binding security, explicitly configured QoS and mismatched server identity. Verify whether rejection happens at binding, connection or invocation, and distinguish endpoint DACL from method authorization.
- On Linux, test pathname and abstract `AF_UNIX` endpoints under separate host UIDs, `SO_PEERCRED`, `SO_PASSCRED` and `SCM_RIGHTS`; assert that host authentication is not accidentally accepted as guest token authorization.
- Exercise cancellation, disconnect, server crash, impersonation revert, and replayed/stale endpoint registration. Instrument guest-handle and host-fd lifetimes independently.

## Decision gate

Do **not** select an ALPC-equivalent Linux kernel extension or claim RPC compatibility on the basis of socket throughput. First obtain a Windows-reference RPC identity/endpoint matrix; then implement a confined user-space binding prototype and compare correctness and overhead. Only after those results evaluate whether a kernel primitive is justified.

## Source and evidence scope

Microsoft Learn documentation and Linux man-pages were reviewed on 2026-11-20; they describe public API behavior rather than pinned Windows internals or a particular Linux kernel commit. No code imported, no benchmarks run, no NT API implemented. Community ALPC descriptions were not used as normative evidence.
