# R175 — COM apartment identity, message pumping and marshaling gates

Date: 2026-11-18. Evidence class: **primary-documentation review only**. **No Windows or Linux experiment was executed.** This is a proposed compatibility boundary, not an implementation claim.

## Question

Can an NT-on-Linux user-mode runtime support COM clients merely by exporting CoInitializeEx and translating blocking calls to Linux waits? **No as an architectural inference:** the documented COM contract includes thread-bound apartment identity, cross-apartment proxies and STA message dispatch. An exported function name and Linux mutex alone do not implement those behaviors.

## VERIFIED — documented Microsoft contracts

1. **Per-thread initialization and balanced teardown.** Each COM-using thread initializes separately using CoInitializeEx; successful initialization calls require corresponding CoUninitialize calls. Apartment-threaded callers promise a message loop. [Microsoft, Initializing the COM Library](https://learn.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library).
2. **Apartment membership is observable.** A process can contain multiple STAs but at most one MTA. An object belongs to one apartment; callers in other apartments require proxy-based access rather than an unchecked raw pointer call. STA calls are dispatched through the receiving apartment's message processing. [Microsoft, Processes, Threads, and Apartments](https://learn.microsoft.com/en-us/windows/win32/com/processes--threads--and-apartments).
3. **Mode transitions can fail.** CoInitializeEx documents RPC_E_CHANGED_MODE for incompatible concurrency-model transitions, including an NTA-related case; OleInitialize selects STA. Do not silently switch an already initialized thread's model. [Microsoft, CoInitializeEx](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex).
4. **In-process class registration affects activation.** The InprocServer32 ThreadingModel setting participates in choosing an apartment for object creation; Apartment, Free and Both are not interchangeable. [Microsoft, Choosing the Threading Model](https://learn.microsoft.com/en-us/windows/win32/com/choosing-the-threading-model).
5. **STA blocking is not a generic Linux wait.** Microsoft's apartment guidance warns that an STA thread blocked without message pumping can prevent incoming calls and deadlock. The documented alternatives include COM-aware or message-aware waits. [Microsoft, Processes, Threads, and Apartments](https://learn.microsoft.com/en-us/windows/win32/com/processes--threads--and-apartments).

## HYPOTHESIS — candidate architecture, not selected

A user-space COM compatibility layer would maintain a **guest per-thread apartment record**, process-wide MTA identity, object/apartment ownership, marshaled proxy identity, and an STA dispatch queue integrated with the guest Win32 message subsystem. Host pthread identity may help implement thread ownership but is not a substitute for the guest COM contract. A generic Linux eventfd/epoll transport might wake an STA; it cannot by itself provide COM call ordering, message-filter behavior, reentrancy, proxy lifetimes or HRESULT outcomes.

The message pump is a **dependency on a functional guest USER32 message model**; this raises COM from a simple library-export backlog item to a cross-subsystem compatibility gate. Cross-process COM additionally needs activation, identity, marshaling and security design, which this note does not specify.

## OPEN — versioned Windows-reference conformance matrix

Run separate, instrumented Windows tests on explicitly recorded Windows builds, bitness and class registrations before choosing implementation behavior:

- Initialize a thread STA twice, unwind twice, and compare success codes and post-uninitialize behavior; separately attempt STA→MTA and MTA→STA to capture RPC_E_CHANGED_MODE.
- Create an STA object and invoke it from an MTA through documented marshaling; verify callback execution thread and whether a deliberately unpumped STA blocks. Use watchdog timeouts and isolated child processes for deadlock cases.
- Compare activation of test in-process servers registered Apartment, Free and Both; record object-creation thread, call thread and whether a proxy is involved.
- Exercise nested inbound calls during outbound STA calls to establish permitted reentrancy and teardown ordering; avoid assuming all callbacks can be safely serialized.
- Compare COM-aware waits against raw blocking waits with an incoming call queued to the STA. Record HRESULTs, ordering and elapsed time; do not infer results without running the reference.
- Test per-thread teardown while outstanding marshaled proxies and queued callbacks exist; record actual observed lifetime boundaries.

## Safety, performance and licensing gates

Do not execute untrusted guest COM server DLLs in the research harness. Treat COM class registration, activation paths and proxy/stub loading as guest-controlled input requiring confinement and policy checks. Do not use Linux host file permissions as a stand-in for COM authorization. A future implementation must measure dispatch latency and deadlock freedom separately from API compatibility; there is currently **no performance evidence**. Microsoft documentation is used as specification evidence; no Microsoft or third-party implementation code was copied.

## Relationship to existing work

R35/R165 address fibers/FLS and thread identity, R40 addresses console input, R159 activation contexts, and R92 guest access checks. This note specifically adds **COM apartment membership, cross-apartment call delivery and STA pumping**; it does not claim those other subsystems are solved.
