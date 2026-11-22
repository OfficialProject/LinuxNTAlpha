# R243 — NT object-manager symbolic links are not Linux filesystem symlinks

Research date: 2026-11-22. Status: **documentation review only; no experiment or implementation**. Scope: the NT object-manager namespace, not NTFS reparse points (R60) or DOS-device logon mappings (R230).

## Verified from primary contracts

1. Microsoft distinguishes **object directories** from filesystem directories: object directories contain named kernel objects and do not correspond to disk directories. The documented `\\Device` and `\\DosDevices` directories contain device objects and DOS-name symbolic links respectively. [Microsoft: Object Directories](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/object-directories).
2. The native symbolic-link object has its own handle/open/query interface. `NtOpenSymbolicLinkObject` opens the link as an object; `NtQuerySymbolicLinkObject` retrieves its Unicode target, with `STATUS_BUFFER_TOO_SMALL` and a required buffer length documented. These are not `readlink(2)` contracts. [Microsoft: NtOpenSymbolicLinkObject](https://learn.microsoft.com/en-us/windows/win32/devnotes/ntopensymboliclinkobject); [Microsoft: NtQuerySymbolicLinkObject](https://learn.microsoft.com/en-us/windows/win32/devnotes/ntquerysymboliclinkobject).
3. `OBJECT_ATTRIBUTES` explicitly defines `OBJ_OPENLINK` to open a symbolic-link object rather than its referent when applicable. It separately defines `OBJ_DONT_REPARSE` as refusing reparses while parsing the object name, returning `STATUS_REPARSE_POINT_ENCOUNTERED`. **The general definition does not establish the precise user-mode NtCreateFile interaction with object-manager links versus filesystem reparse points.** [Microsoft: OBJECT_ATTRIBUTES](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wudfwdm/ns-wudfwdm-_object_attributes).
4. `ZwCreateDirectoryObject` documents object-directory-specific access rights (`DIRECTORY_QUERY`, `DIRECTORY_TRAVERSE`, `DIRECTORY_CREATE_OBJECT`, `DIRECTORY_CREATE_SUBDIRECTORY`); it expressly distinguishes these objects from filesystem directories. [Microsoft: ZwCreateDirectoryObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwcreatedirectoryobject).
5. Session-local and global kernel-object naming have privilege-sensitive creation rules; creation of a symbolic-link object in the global namespace from a nonzero session requires `SeCreateGlobalPrivilege`, while opening an existing global object is governed by its access rights. [Microsoft: Kernel object namespaces](https://learn.microsoft.com/en-us/windows/win32/termserv/kernel-object-namespaces).

## Architecture hypothesis (NOT verified)

Implement a **guest-owned, typed object-namespace resolver** ahead of host filesystem path translation. Each namespace node has an NT object type, parent-directory identity, security descriptor, name and lifetime; a symbolic-link node stores a Unicode target and is addressable independently of its referent. A resolution operation receives caller security context, root-directory handle, requested type, object attributes and namespace/session context. Only a successfully resolved guest file/device boundary is handed to a host filesystem backend. Host symlinks may be a storage detail but cannot define the guest object-manager namespace.

This avoids treating `\\DosDevices\\C:` as a host path or assuming `openat2` can resolve NT object links. R230 covers the caller's DOS-device mapping and R60 covers on-disk reparse points; neither alone establishes object-manager link handle identity and access checks.

## Unresolved conformance gates (all UNRUN)

- **Link vs target:** create/open/query a test link; compare ordinary resolution with `OBJ_OPENLINK`; check type mismatch, target absence, relative roots, buffer lengths and required access rights. Use a disposable Windows reference environment and document exact OS build.
- **Traversal:** nested links, cycles, link to missing target, link to a directory versus file, and link to a device namespace. Record termination statuses and whether limits are documented or empirical; never assume an arbitrary depth.
- **Authorization:** deny access to the link object but allow the target (and inverse), directory traversal versus object opening, guest impersonation, session-local shadowing and global creation privilege.
- **Lifetime and races:** duplicate handles to a link; remove or replace its namespace name while a handle remains; concurrently resolve while another thread mutates names. Determine reference lifetime separately from name lifetime.
- **Flag ambiguity:** compare `OBJ_DONT_REPARSE` with `OBJ_OPENLINK` and filesystem `FILE_OPEN_REPARSE_POINT` for object links and on-disk reparse points. The Microsoft general structure page is insufficient to assert universal behavior for every service; pin the tested Windows build.
- **Host isolation:** prove that an untrusted guest target cannot escape a permitted host root by inserting NT-style aliases or Linux symlinks. Do not equate guest security checks with host path confinement.

## Decision consequence

Do **not** collapse NT object-directory names, symbolic-link objects, DOS-device mappings and filesystem reparse points into one Linux pathname table. Keep the mapping between guest object identity and host backend resources explicit. This is an architectural recommendation requiring implementation and Windows-reference validation, not a selected ADR-003 architecture.

## Source and evidence limits

Sources are official Microsoft API/WDK documentation retrieved 2026-11-22. No upstream code revision was inspected for this note. No Windows reference machine, Linux harness, performance measurement or security test was run. The documentation for native Nt/Zw entry points includes kernel-mode-specific instructions; those must not be copied into user-mode call conventions without validation.
