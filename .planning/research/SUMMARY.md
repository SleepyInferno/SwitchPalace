# Research Summary: SwitchPalace

**Project:** SwitchPalace - Open-source Nintendo Switch content manager
**Domain:** Nintendo Switch homebrew (NRO + PC companion)
**Researched:** 2026-03-25
**Confidence:** MEDIUM-HIGH

---

## Recommended Stack

The Switch NRO is built with **devkitA64 + libnx (v4.11.1)** -- the only toolchain for Switch homebrew -- using **borealis (xfangfang fork)** for a native Nintendo-style UI with XML layouts and controller navigation. All content installation goes through libnx's NCM/ES/NS services, which provide a streaming placeholder-based write API that enables install-while-downloading.

The PC companion server uses **Go** over Rust or Python. The workload is I/O-bound (USB bulk transfers, HTTP file serving), where Go performs comparably to Rust. Go wins on contributor accessibility, trivial cross-compilation to single binaries, and sufficient performance -- DBI's Python backend is the bottleneck SwitchPalace replaces, and Go is 10-50x faster than Python for I/O. USB communication uses **google/gousb**; HTTP serving uses Go's stdlib.

CI runs on **GitHub Actions** with the official `devkitpro/devkita64` Docker image for NRO builds and native Go cross-compilation for the PC server.

---

## Table Stakes Features

Without every one of these, users stay on DBI:

- **Install NSP/XCI/NSZ/XCZ from SD card** -- core purpose, including split files for FAT32
- **USB install via PC companion** (custom protocol, faster than MTP)
- **MTP Responder mode** -- DBI's most popular feature; PC sees Switch as USB drive
- **HTTP/HTTPS network install** with Tinfoil shop protocol compatibility
- **FTP and SFTP server installs** -- users host content on local servers
- **Browse, delete, and move installed titles** between SD and NAND
- **Ticket management** -- view and delete eTickets
- **Save backup and restore** to SD card, per-game and per-user
- **SD card and NAND file browser** with copy/move/delete
- **Per-game play time and activity stats** (via pdm service)
- **Batch/multi-select installation** with progress display
- **FTP server mode** on Switch for wireless file management

---

## Key Differentiators

What makes SwitchPalace worth switching from DBI:

1. **Speed** -- Parallel chunked HTTP downloads (multi-stream vs DBI's single-stream), optimized USB protocol targeting 100+ MB/s (vs DBI's ~25 MB/s MTP), and a Go companion server replacing DBI's slow Python backend
2. **Trust and transparency** -- Fully open source, no obfuscation, reproducible builds, no telemetry. DBI is closed-source with XOR-obfuscated strings and a developer who has threatened brick code. This is the emotional reason users switch.
3. **Modern UX** -- borealis gives a native Switch system menu feel. Search/filter/sort for large libraries (DBI has no search). Clear error messages instead of cryptic hex codes.
4. **Cross-platform PC companion** -- Single Go binary for Windows/Mac/Linux, CLI-first with future GUI. Fast startup vs DBI's Python overhead.

---

## Architecture Highlights

**Two-component system:** Switch NRO (C/C++ on libnx) handles UI, installation, and device management. PC companion (Go) handles file serving over USB and HTTP.

**Build order driven by dependencies:**

1. **NCA installation pipeline first** -- Everything depends on it. Streaming placeholder writes (1-4MB chunks) mean you never buffer entire NCAs in memory. Ticket import must happen before NCA registration.
2. **USB protocol second** -- Simpler than network (no DNS/TLS), provides the fastest test loop. Implement DBI0-compatible protocol (16-byte headers, 1MB chunks, bulk endpoints). PC Go server should be built slightly ahead of Switch client.
3. **Network install third** -- HTTP client with Range requests enables parallel chunked downloads (the main speed differentiator). Producer-consumer pattern: download workers fill a reorder buffer, installation writer consumes chunks sequentially.
4. **MTP + file management fourth** -- MTP responder is complex (full protocol, OS-specific quirks). Use mtp-server-nx as reference. Virtual "Install" folder triggers installation pipeline.
5. **Content management + polish last** -- Title listing, save backup, tickets, activity stats. These use already-built service wrappers.

**Key patterns:** Streaming installation (never buffer whole NCAs), double-buffered USB transfers, decoupled UI updates (UI thread polls shared state, never blocks on I/O), fixed thread pool (4-6 workers max, 12 TLS slots total), service sessions opened on demand and closed promptly.

---

## Watch Out For

1. **Orphaned NCA placeholders (CP-1)** -- If installation is interrupted between CreatePlaceholder and Register, orphan files waste storage invisibly. Implement transaction-like rollback: track state, call DeletePlaceholder on any failure. Scan for orphans on startup.

2. **USB 16MB transfer boundary hang (DM-2)** -- Transfers near 0x1000000 bytes silently hang the Switch USB stack. Cap individual transfers at 8MB. All USB buffers must be 0x1000-byte aligned with dcache flushed before PostBufferAsync.

3. **exFAT filesystem corruption (CP-4)** -- Nintendo's exFAT driver is buggy; crashes during writes can destroy the single file allocation table. Detect exFAT at runtime and show persistent warnings. Recommend FAT32. Support FAT32's 4GB split-file convention (directory with numbered parts).

4. **Applet mode memory starvation (DM-1)** -- Album applet mode gives only ~442MB RAM vs ~3.7GB in title override. Detect mode with `appletGetAppletType()`, adapt buffer sizes, disable parallel downloads in applet mode. Display mode indicator. Test in both modes.

5. **DMCA and key material (LC-1, LC-2)** -- Never include Nintendo's cryptographic keys or key derivation code. Use libnx's `splCrypto` services so keys never leave the secure processor. Frame the project as "content management." Host mirrors outside GitHub as contingency.

---

## Open Questions

- **Tinfoil shop protocol specifics** -- Not formally specified; must be reverse-engineered from live servers. How much has it drifted since last community documentation?
- **USB 3.0 throughput ceiling with gousb** -- google/gousb is proven for bulk transfers but untested at maximum USB 3.0 speeds on Switch. Need early profiling.
- **NSZ decompression performance on Switch ARM cores** -- Zstandard decompression is CPU-bound. What is the practical throughput? Does it bottleneck faster-than-WiFi sources?
- **borealis memory footprint in applet mode** -- Can a full borealis UI with lazy-loaded game library (500+ titles) fit in 442MB alongside installation buffers?
- **MTP cross-platform compatibility** -- Windows, macOS, and Linux MTP clients behave differently. How much per-OS workaround code is needed?
- **Reproducible NRO builds** -- devkitPro Docker image should enable this, but has anyone actually verified bitwise reproducibility?
- **License choice** -- GPL vs MIT has implications for community forks and Nintendo's legal posture. Needs a deliberate decision before public release.

---

*Research completed: 2026-03-25*
*Ready for roadmap: yes*
