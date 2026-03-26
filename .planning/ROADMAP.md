# Roadmap: SwitchPalace

## Overview

SwitchPalace replaces DBI with a fast, open-source, two-component system: a Switch NRO (C/C++ on libnx) and a Go PC companion server. The roadmap follows the critical dependency chain identified in research: NCA installation pipeline first (everything depends on it), USB protocol second (simplest transport, fastest test loop), network install third (primary speed differentiator), then MTP/file/game management, and finally saves/tickets/activity stats.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation & Install Pipeline** - devkitPro toolchain, borealis UI scaffold, NCA streaming install for all formats, trust infrastructure
- [ ] **Phase 2: PC Companion & USB Install** - Go CLI server, custom USB protocol, optimized chunked transfers, driver guidance
- [ ] **Phase 3: Network Install** - HTTP/FTP/SFTP/Tinfoil sources, parallel chunked downloads, resume support, source configuration
- [ ] **Phase 4: MTP, File & Game Management** - MTP responder with virtual drives, SD/NAND file browser, installed title management
- [ ] **Phase 5: Saves, Tickets & Activity** - Save backup/restore, ticket viewer/deletion, per-game activity stats

## Phase Details

### Phase 1: Foundation & Install Pipeline
**Goal**: Users can install any supported game format (NSP, XCI, NSZ, XCZ) from SD card to their chosen destination with integrity verification and safe rollback on failure, built on a fully open and trustworthy codebase
**Depends on**: Nothing (first phase)
**Requirements**: INST-01, INST-02, INST-03, INST-04, INST-05, INST-06, TRUST-01, TRUST-02, TRUST-03, TRUST-04
**Success Criteria** (what must be TRUE):
  1. User can select an NSP, XCI, NSZ, or XCZ file from SD card and install it to either SD or NAND
  2. User can see a hash verification result during installation confirming file integrity
  3. If the user powers off or cancels mid-install, no orphaned placeholders remain on the system
  4. User can clone the repository, run the documented build, and produce an identical NRO binary
  5. App displays whether it is running in applet mode or application mode and adapts accordingly
**Plans**: 3 plans

Plans:
- [ ] 01-01-PLAN.md — Project scaffold, build system, borealis integration, container parsers, type contracts
- [ ] 01-02-PLAN.md — NCA install pipeline engine (NCM wrapper, NCA writer, NCZ decompression, NSP/XCI installers)
- [ ] 01-03-PLAN.md — UI screens (file browser, progress, summary, dialogs) + integration wiring + hardware verification

### Phase 2: PC Companion & USB Install
**Goal**: Users can install games from their PC to the Switch over USB using the SwitchPalace companion, with optimized transfer speeds and clear driver setup guidance
**Depends on**: Phase 1
**Requirements**: USB-01, USB-02, USB-03, USB-04, USB-07
**Success Criteria** (what must be TRUE):
  1. User can run the Go companion server on Windows/Mac/Linux, connect the Switch via USB, and install a game file from PC to Switch
  2. User can use either the CLI or the desktop GUI to browse local files and send them to the Switch
  3. USB transfer speed is noticeably faster than DBI's Python backend (targeting sustained throughput with double-buffered chunking below the 16MB boundary)
  4. Windows user who has never used the tool can follow the bundled guide to install the correct USB driver and connect successfully
**Plans**: TBD

Plans:
- [ ] 02-01: TBD
- [ ] 02-02: TBD

### Phase 3: Network Install
**Goal**: Users can browse, download, and install games from remote HTTP, FTP, SFTP, and Tinfoil-compatible servers with fast parallel downloads and resume capability
**Depends on**: Phase 1
**Requirements**: NET-01, NET-02, NET-03, NET-04, NET-05, NET-06, NET-07
**Success Criteria** (what must be TRUE):
  1. User can add a named HTTP/FTP/SFTP server or Tinfoil shop URL in a config file and browse its contents from the Switch
  2. User can install a game from any configured network source and see it complete with parallel chunked downloading
  3. If a network install is interrupted (WiFi drop, sleep mode), the user can resume the download without starting over
  4. Network download speed is measurably faster than DBI for the same source (parallel Range requests vs single-stream)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD
- [ ] 03-02: TBD

### Phase 4: MTP, File & Game Management
**Goal**: Users can manage their Switch as a USB drive from PC via MTP, browse and manipulate files on SD/NAND, and manage installed game titles
**Depends on**: Phase 2
**Requirements**: USB-05, USB-06, MGMT-01, MGMT-02, MGMT-03, MGMT-04, FILE-01, FILE-02, FILE-03, FILE-04, FILE-05
**Success Criteria** (what must be TRUE):
  1. User can connect the Switch to a PC and it appears as an MTP device with virtual drives for SD card, installed games, and saves
  2. User can browse SD card and NAND partitions in the built-in file manager, and copy/move/delete files (with NAND system as read-only)
  3. User can view all installed titles with metadata, move titles between SD and NAND, delete individual base/update/DLC entries, and run integrity checks
  4. User can access Switch files wirelessly from a PC via the built-in FTP server
  5. Files larger than 4GB are automatically split for FAT32 compatibility
**Plans**: TBD

Plans:
- [ ] 04-01: TBD
- [ ] 04-02: TBD
- [ ] 04-03: TBD

### Phase 5: Saves, Tickets & Activity
**Goal**: Users can back up and restore save data, manage eTickets, and view per-game activity statistics
**Depends on**: Phase 1
**Requirements**: SAVE-01, SAVE-02, SAVE-03, ADV-01, ADV-02, ADV-03, ADV-04
**Success Criteria** (what must be TRUE):
  1. User can back up save data for any game and any user account to SD card, covering all save types (Account, Device, BCAT, Temporary, Cache)
  2. User can restore a save backup from SD card without needing to launch the game first
  3. User can view all installed tickets with type labels (personalized vs common) and delete individual tickets
  4. User can view per-game play time and launch count, broken down by user account
**Plans**: TBD

Plans:
- [ ] 05-01: TBD
- [ ] 05-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5
Note: Phases 2, 3, and 5 all depend on Phase 1 but not on each other. Phase 4 depends on Phase 2.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation & Install Pipeline | 1/3 | In Progress|  |
| 2. PC Companion & USB Install | 0/2 | Not started | - |
| 3. Network Install | 0/2 | Not started | - |
| 4. MTP, File & Game Management | 0/3 | Not started | - |
| 5. Saves, Tickets & Activity | 0/2 | Not started | - |
