# Architecture decisions

## ADR-000: Research-first development
Status: ACCEPTED (project direction)
Decision: Defer permanent kernel and runtime architecture choices until source audits and controlled experiments are complete.
Reason: Native Windows execution requires more than PE loading; performance claims must be measured.
Consequences: Begin with an upstream Linux kernel for experiments. No Fedora/Bazzite/SteamOS dependency is committed.

## ADR-001: Runtime independence
Status: REQUIREMENT, NOT IMPLEMENTED
Decision: Target no Wine/Proton runtime dependency. Study existing projects as references and evaluate legally compatible reuse.
Clarification: A Windows-compatible API/NT implementation remains necessary; direct CPU execution is not synonymous with API-free execution.

## ADR-002: Product sequencing
Status: ACCEPTED
Decision: NT subsystem first; desktop OS after; console-style fork only after desktop completion.

## ADR-003: Kernel boundary
Status: OPEN
Candidates: user-space services; targeted Linux extensions; deeper NT integration.
Selection criteria: demonstrated performance, correctness, security, maintainability, and driver compatibility.
