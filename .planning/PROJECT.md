# SwitchPalace

## What This Is

SwitchPalace is an open-source Nintendo Switch homebrew content management application that fully replaces DBI (DB Installer). It consists of two components: a Switch NRO app that runs on the console and a high-performance PC companion server. It is designed for the modded Switch community and will be publicly released.

## Core Value

A fast, fully open-source, and trustworthy Switch content manager — users can audit every line of code and never worry about malicious behavior, while getting noticeably faster installs than DBI.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Switch NRO app runs on Atmosphere CFW via devkitPro/libnx (C/C++)
- [ ] PC companion server prioritizes raw transfer speed over aesthetics
- [ ] Network install: browse and install from HTTP, FTP, SFTP, and community shop servers
- [ ] USB/MTP transfer: drag-and-drop game installation from PC over USB
- [ ] Game management: view installed titles, move between SD/NAND, delete
- [ ] Save backup and restore per game and per user account
- [ ] File manager: browse SD card and NAND partitions
- [ ] FTP server mode on the Switch (for wireless file access)
- [ ] Ticket management: view and delete tickets
- [ ] Activity log: per-game play time and launch statistics
- [ ] Parallel/chunked downloads for maximum network throughput
- [ ] Open source (fully auditable — no obfuscation)

### Out of Scope

- Built-in piracy storefront — users configure their own servers (same model as DBI)
- Mobile companion app — PC-first
- Real-time chat or social features — not a community platform

## Context

- DBI (the target to replace) is closed-source with XOR-obfuscated strings and unverified developer threats of brick-triggering code in translated builds. This is the primary motivation for SwitchPalace.
- DBI's slow download speed stems from sequential (non-parallel) HTTP transfers and an inefficient USB protocol. SwitchPalace will use parallel chunk downloading and a faster USB stack.
- The Switch homebrew ecosystem uses devkitPro (devkitA64 toolchain) with libnx for NRO development. The standard UI toolkit is borealis or custom LVGL-based UIs.
- The PC companion currently (DBI's dbibackend) is Python with pyusb — we will replace with a high-performance Go or Rust server.
- Community servers (Tinfoil-format shops) use a JSON/URLList protocol that SwitchPalace must support.
- Target platform: Nintendo Switch running Atmosphere CFW with ES sigpatches.

## Constraints

- **Platform**: Switch homebrew requires devkitPro (devkitA64) + libnx — no alternative toolchain
- **CFW dependency**: App requires Atmosphere CFW; out of scope to support stock firmware
- **Protocol compatibility**: Must support Tinfoil shop protocol (URLList/JSON) for community server interoperability
- **USB driver**: Windows users need libusbK or WinUSB drivers — PC companion must document this
- **Open source**: All code must be fully open and auditable — no obfuscation ever

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Go or Rust for PC companion | Raw speed priority; both are significantly faster than Python for I/O-heavy server work | — Pending |
| Parallel chunked HTTP downloads | Primary fix for DBI's slow network installs | — Pending |
| borealis vs custom UI for NRO | borealis is the standard Nintendo-style UI lib for Switch homebrew | — Pending |

---
*Last updated: 2026-03-25 after initialization*
