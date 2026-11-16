# R127 — PE resource tree parsing and language-selection gates

Date: 2026-11-16. Status: **documentation review; no experiment executed**.

## Scope and relationship to prior work

R19/R23/R44 establish narrow PE execution and parse-only gates; R29/R32/R106 cover API-set, DLL search, and activation contexts. This note addresses a distinct unresolved surface: PE resource-directory traversal and Win32 resource lookup. It does **not** assert that the current loader implements resources or MUI.

## Verified primary-source contracts

1. **Resource directory structure.** The Microsoft PE format defines a resource tree conventionally organized as **type → name → language**. Each directory header records separate named-entry and ID-entry counts; named entries precede ID entries and entries are sorted within their respective groups. Directory-entry offsets are relative to the resource directory base. The high bit in a child pointer distinguishes a subdirectory from a leaf data-entry reference. A leaf data description identifies the resource data's location and size. These are on-disk parsing rules, not an instruction to dereference unchecked guest addresses. [S1]
2. **Identifiers are not always strings.** Win32 FindResourceW and FindResourceExW accept either names or integer identifiers (MAKEINTRESOURCE); their documentation also describes decimal `#number` strings. An implementation must preserve the name/ID distinction rather than normalizing everything to filenames. [S2, S3]
3. **Language selection is API-visible.** FindResourceExW explicitly accepts a WORD language identifier; the neutral/neutral language combination uses the language associated with the calling thread. FindResourceW lacks an explicit language parameter. Both APIs can search a module's PE image **or an accompanying MUI file**. The exact cross-version fallback order is **not established** by these two API pages and must not be inferred from directory ordering. [S2, S3]
4. **Resource bytes are not always the final application representation.** Microsoft recommends resource-specific APIs for common types; FindResourceExW documents string resources stored in blocks of up to sixteen counted UTF-16 strings. A generic binary resource lookup and LoadString semantics are distinct features. [S2]

## Architecture hypotheses (not implementation decisions)

- **H1:** A read-only, bounded resource-tree index per loaded module can serve ordinary resource lookup without modifying Linux kernel APIs. Module identity, lifetime and MUI redirection would remain guest-runtime concerns.
- **H2:** Keep PE directory parsing separate from language-selection policy. This avoids hardcoding a Windows-version-dependent fallback order into the parser and permits reference-tested behavior later.
- **H3:** Preserve raw resource data and resource identity, then implement type-specific conversion at higher API layers. A host filesystem path or Linux locale is not itself an NT resource identifier or Windows thread language policy.

## Proposed gates — NOT RUN

**Parser safety:** parse-only fixtures with valid numeric and named type/name/language paths; empty tables; mixed name/ID entries; truncated 16-byte headers, 8-byte entries, UTF-16 names and leaf descriptors; arithmetic overflow in entry counts; out-of-range relative offsets and RVAs; cycles/shared subdirectories; excessive depth and node counts; resource data crossing mapped image bounds. Reject malformed fixtures deterministically and cap traversal work. Treat handling of noncanonical yet Windows-accepted trees as an explicit compatibility question rather than silently assuming the specification's conventional depth is an absolute OS limit.

**Windows-reference behavior:** compile version-pinned PE fixtures with multiple language variants and named/numeric IDs. Compare FindResourceW, FindResourceExW (explicit language and neutral/neutral), SizeofResource, LoadResource/LockResource and LoadStringW against the same fixtures on documented Windows versions. Record thread UI-language configuration, installed MUI resources, exact error codes, and module loading mode. Include a module with an accompanying MUI resource to test separation of PE parsing from language fallback. Do not claim results until executed.

**Lifetime/security:** check lookup against unloaded modules, invalid module handles, zero-sized resources, and concurrent unload; test that hostile resource offsets cannot escape image bounds. Determine whether module resource data pointers remain usable across unload through Windows-reference tests rather than relying on host mapping intuition.

## Open questions

1. Which Windows versions and MUI language fallback policies are in project scope?
2. Does the loader's eventual image mapping expose resource data by RVA consistently for image-versus-datafile loading?
3. Which resource types are essential for the first integrated PE milestone versus later UI support?
4. What exact resource-directory malformed-input acceptance does Windows exhibit, and should LinuxNTAlpha deliberately be stricter?

## Primary sources (retrieved 2026-11-16)

- [S1] Microsoft, *PE Format*, “The .rsrc Section”, resource directory tables/entries/data: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- [S2] Microsoft, *FindResourceExW*: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-findresourceexw
- [S3] Microsoft, *FindResourceW*: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-findresourcew

No Linux or Windows experiment was performed for R127. No benchmark, implementation, or compatibility claim follows from this review.
