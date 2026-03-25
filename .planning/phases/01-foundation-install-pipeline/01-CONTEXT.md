# Phase 1: Foundation & Install Pipeline - Context

**Gathered:** 2026-03-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Build a Switch NRO app on devkitPro/libnx with borealis UI that lets users install NSP, XCI, NSZ, and XCZ game files from SD card to a chosen destination (SD card or NAND), with hash integrity verification and safe atomic rollback on failure or cancellation. Trust infrastructure includes reproducible builds and applet/application mode detection. No network installs, no USB, no game management — those are separate phases.

</domain>

<decisions>
## Implementation Decisions

### File Selection
- Recursive scan of all SD card subdirectories — show all .nsp, .xci, .nsz, .xcz files in a flat list
- Files sorted alphabetically by filename
- Multi-select enabled: user can queue multiple files for batch install in one go

### Install Destination
- User chooses SD card or NAND per batch, after selecting files and before install begins
- Prompt shown once for the entire queue (not per file)

### Conflict Handling
- If a title is already installed at the destination: fail with a clear message telling the user to delete the existing version first
- No silent overwrites — explicit user action required before reinstalling

### Hash Verification
- Mandatory for every install — cannot be skipped
- Hash verification result shown inline during the progress view (checkmark or ✘ per file as it completes)
- Also shown on the post-install summary screen

### Install Progress Display
- Overall batch progress bar (%) + current filename shown below the bar
- Transfer speed (MB/s) and ETA displayed
- Cancel button available during install — pressing it shows a confirmation dialog warning that the partial install will be rolled back (NCM placeholders removed, INST-06 rollback)

### Post-Install Summary
- After batch completes: summary screen listing each file with pass/fail result
- User dismisses with B/confirm to return to the file list

### Applet Mode Behavior
- App detects and displays a persistent "APPLET" badge in a corner of the main screen when running as an applet
- In applet mode: NSZ and XCZ installs are disabled (decompression is memory-intensive and unsafe in applet memory budget)
- Disabled items shown grayed with a label indicating they require application mode
- NSP and XCI installs remain available in applet mode

### Claude's Discretion
- Heap allocation strategy: conservative caps in applet mode, generous in application mode — Claude picks specific values
- NSZ/XCZ decompression buffer sizing and Zstandard library integration
- borealis layout details, spacing, and animation choices
- NCM placeholder management implementation details for rollback

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Requirements
- `.planning/REQUIREMENTS.md` — INST-01 through INST-06 (installation requirements), TRUST-01 through TRUST-04 (trust/quality requirements)
- `.planning/ROADMAP.md` §Phase 1 — Goal, success criteria, and plan breakdown for this phase

### No external specs
No external ADRs or design docs yet — all requirements are captured in REQUIREMENTS.md and decisions above.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- None yet — greenfield project. No existing components, hooks, or utilities.

### Established Patterns
- None yet — this phase establishes the patterns all future phases will follow.

### Integration Points
- devkitPro/devkitA64 toolchain + libnx: the only valid toolchain for Switch NRO development
- borealis (xfangfang fork): UI framework; all screens built as borealis views
- Atmosphere CFW: required runtime environment — app can assume Atmosphere is present
- NSZ/XCZ decompression: Zstandard library (bundled or devkitPro-provided)

</code_context>

<specifics>
## Specific Ideas

No specific UI references or "I want it like X" moments — open to standard borealis patterns for layout and interaction design.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 01-foundation-install-pipeline*
*Context gathered: 2026-03-25*
