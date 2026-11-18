# R167 — Windows delayed clipboard rendering versus Wayland selection ownership

Status: PRIMARY-SOURCE REVIEW; no implementation, Windows reference run, or Linux experiment.
Reviewed: 2026-11-18. Scope: desktop compatibility boundary, not NT kernel syscall semantics.
Novelty relative to R134: R134 concerns job UI restrictions and authority; this study concerns clipboard owner callback and transfer lifetimes.

## Verified contracts

1. **Windows clipboard ownership and deferred formats.** A window becomes clipboard owner through EmptyClipboard. SetClipboardData with a NULL data handle advertises a format for delayed rendering; the owner must service WM_RENDERFORMAT when another application requests that format. Microsoft explicitly prohibits OpenClipboard inside WM_RENDERFORMAT: the requester already holds the clipboard open. [W1][W2]
2. **Owner destruction is a separate transaction.** WM_RENDERALLFORMATS is delivered before destruction to permit materializing deferred formats. Unlike WM_RENDERFORMAT, the owner must open the clipboard and confirm that it still owns it; unrendered formats disappear after the callback. [W3][W1]
3. **Sequence identity is window-station scoped.** GetClipboardSequenceNumber reports the current window station's sequence; missing WINSTA_ACCESSCLIPBOARD returns zero. Microsoft's documentation says delayed rendering postpones increment until rendering. Do not equate a Wayland offer notification with this counter. [W4]
4. **Wayland selection is seat/focus and offer oriented.** Core wl_data_device announces MIME types and selection offers to clients around keyboard focus; set_selection uses an input-event serial. The source transfers requested MIME data via a file descriptor. This is not a Windows window-message callback or a window-station clipboard namespace. [L1][L2]

## Architectural consequence (inference, not measured)

A guest clipboard broker should maintain **guest window-station/desktop ownership, registered formats, deferred-render promises, sequence identity, and message dispatch** separately from an optional Wayland export/import adapter. On guest-to-host copy, the broker can advertise available MIME types without eagerly rendering all guest formats, but must preserve a live guest owner callback route and handle owner teardown. On host-to-guest copy, offers need a format-conversion policy and explicit lifetime ownership. Neither mapping is guaranteed by a raw host PTY, pipe, or Wayland clipboard offer.

The broker must not treat host clipboard access as a privilege bypass: host compositor selection visibility and guest WINSTA_ACCESSCLIPBOARD are distinct authorities. **Security hypothesis:** cross-boundary export should require an explicit session policy and enforce format/size limits; test against a hostile guest that repeatedly advertises large or recursively converted formats. The primary contracts alone do not establish a secure implementation.

## Unresolved questions and proposed gates (NOT EXECUTED)

- Windows-reference: two processes, separate windows; advertise CF_UNICODETEXT deferred, then GetClipboardData; record WM_RENDERFORMAT dispatch, clipboard open ownership and format payload.
- Destroy owner before and after rendering; verify WM_RENDERALLFORMATS and surviving formats. Race ownership change while teardown is pending.
- Query sequence number before/after EmptyClipboard, deferred SetClipboardData, actual render and ownership replacement; record observed OS build rather than assume a universal increment sequence.
- Compare clipboard access across guest window stations/desktops and distinct host Wayland seats; validate that denied guest access cannot be bypassed through host selection.
- Wayland integration: test focus changes, offer cancellation, owner exit, blocked readers, MIME conversion, clipboard manager presence/absence and bidirectional feedback loops.
- Verify behavior on Windows 10/11 and at least two compositor implementations. Record exact protocol versions and configuration. No outcome is asserted here.

## Sources (primary contracts; accessed 2026-11-18)

- [W1] Microsoft, Clipboard Operations: https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard-operations
- [W2] Microsoft, WM_RENDERFORMAT: https://learn.microsoft.com/en-us/windows/win32/dataxchg/wm-renderformat
- [W3] Microsoft, WM_RENDERALLFORMATS: https://learn.microsoft.com/en-us/windows/win32/dataxchg/wm-renderallformats
- [W4] Microsoft, GetClipboardSequenceNumber: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclipboardsequencenumber
- [L1] Wayland core protocol (wl_data_device): https://wayland.app/protocols/wayland
- [L2] Wayland project, protocol/model: https://wayland.freedesktop.org/docs/book/Protocol.html

No source revision was pinned for the living Wayland protocol documentation; pin a protocol XML revision before implementation.
