# R231 — Windows remote-file protocol reporting versus Linux filesystem metadata

Status: Primary-documentation analysis, 2026-11-21. No Windows reference execution, Linux experiment, implementation, or benchmark.

## Verified contracts

1. Microsoft's `GetFileInformationByHandleEx` takes a **file handle**, an information-class selector, a caller-provided buffer, and its size; success is nonzero and failure is zero with `GetLastError`. Its documentation explicitly says the handle should not be a pipe and that class support and information can differ by OS and underlying driver. [Microsoft GetFileInformationByHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex).
2. `FileRemoteProtocolInfo` is a query-only `FILE_INFO_BY_HANDLE_CLASS` selector; it is not an instruction to negotiate a remote protocol. The enum includes separate storage, alignment and file-ID classes. [Microsoft FILE_INFO_BY_HANDLE_CLASS](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ne-minwinbase-file_info_by_handle_class).
3. The returned `FILE_REMOTE_PROTOCOL_INFO` has explicit structure version/size, protocol identification and version fields, flags, generic and protocol-specific reserved space, and SMB-related capability fields. The Microsoft documentation describes the structure as valid for this query, not as a generic property of arbitrary POSIX descriptors. [Microsoft FILE_REMOTE_PROTOCOL_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_remote_protocol_info).
4. The API documentation lists SMB 3.0, transparent failover, scale-out shares, CSVFS and ReFS support in its Windows 8/Server 2012 technology table. This is a **documented compatibility matrix**, not proof that every handle on those systems returns remote-protocol information successfully. [Microsoft GetFileInformationByHandleEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex).

## Architecture inference (unverified)

The NT-compatible file broker should own **guest-visible filesystem provenance and capability policy**, rather than infer Windows remote protocol identity from a Linux file descriptor, mount path, or network address. Host filesystem information can inform a backend adapter but cannot establish that the guest opened an SMB handle, which SMB version/capabilities are advertised, or whether a class is supported. A local Linux directory backed by network storage is not automatically a Windows SMB redirector.

Model each open guest file object with a backend classification, a guest-visible filesystem/provider identity and versioned query capabilities. Validate information class, handle type, rights where applicable, output length and version before publishing the response. Keep reserved bytes initialized; never pass through untrusted host kernel structs as Windows structs. Do not fabricate SMB capability flags merely because the host mount is CIFS.

This is a design candidate, not a finding about exact Windows status/error codes or the necessary guest object layout.

## Falsifiable acceptance gates (not executed)

- **Windows reference:** Query `FileRemoteProtocolInfo` against local NTFS/ReFS, SMB shares with known negotiated dialects, and invalid/pipe/directory handles. Capture success, `GetLastError`, buffer-size boundaries, structure version/size, protocol, flags, and capability fields; record OS build and filesystem/provider.
- **Version boundaries:** Compare supported Windows builds and SMB dialect/server combinations. Determine which fields are zero, stable, provider-dependent or rejected. Do not infer these from the enum alone.
- **Host isolation:** Run the proposed guest query on Linux local, CIFS and FUSE-backed files under the same guest namespace. Ensure no host mount details or server capabilities leak without guest policy and no guest claim of SMB protocol negotiation without an actual guest-facing provider.
- **Concurrency and identity:** Query while the underlying share disconnects/reconnects and after handle duplication; establish whether the Windows reference snapshots protocol identity per open or re-queries it. Avoid guessing.
- **Parser/security:** Reject undersized output buffers safely, initialize every returned byte, and verify no overflow across 32/64-bit guest ABI boundaries.

## Open questions and decision

Exact error translation, whether local files return a distinguished structure or failure, behavior for non-SMB redirectors, and protocol version/flag publication all require Windows differential tests. Until then, **do not implement `FileRemoteProtocolInfo` by blindly copying Linux mount metadata**. This note complements R61 file identity, R79 streams, R193 EAs, and R201 unbuffered I/O without replacing those analyses.
