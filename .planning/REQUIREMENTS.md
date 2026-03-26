# Requirements: SwitchPalace

**Defined:** 2026-03-25
**Core Value:** A fast, fully open-source, and trustworthy Switch content manager — users can audit every line of code and never worry about malicious behavior, while getting noticeably faster installs than DBI.

## v1 Requirements

### Installation

- [ ] **INST-01**: User can install NSP files (base games, DLC, updates) to SD or NAND
- [ ] **INST-02**: User can install XCI files (cartridge dumps) to SD or NAND
- [ ] **INST-03**: User can install NSZ and XCZ files (compressed formats) with on-the-fly decompression
- [ ] **INST-04**: User can choose install destination (SD card vs NAND) before each install
- [ ] **INST-05**: User can verify file integrity via hash check during installation
- [ ] **INST-06**: Installation is atomic — interrupted installs are rolled back, leaving no orphaned NCM placeholders

### Network Install

- [ ] **NET-01**: User can browse and install from HTTP servers with directory listing (Apache/nginx/rclone)
- [ ] **NET-02**: User can configure and browse Tinfoil shop protocol sources (URLList/JSON format)
- [ ] **NET-03**: User can install from FTP servers
- [ ] **NET-04**: User can install from SFTP servers (password and key-based auth)
- [ ] **NET-05**: Network downloads use parallel chunked HTTP (Range requests) for maximum throughput
- [ ] **NET-06**: Network installs can resume after interruption (partial download recovery)
- [ ] **NET-07**: User can configure multiple named network sources in a config file

### USB & PC Companion

- [ ] **USB-01**: User can install games from PC over USB using the SwitchPalace custom USB protocol
- [ ] **USB-02**: PC companion server runs as a fast CLI/daemon (Go) — headless, no GUI required
- [ ] **USB-03**: PC companion provides a desktop GUI for browsing and sending files to the Switch
- [ ] **USB-04**: USB transfers use optimized chunking (below 16MB boundary) and double-buffering
- [ ] **USB-05**: Switch presents as MTP device when connected to PC (drag-and-drop installation)
- [ ] **USB-06**: MTP virtual drives expose SD card, installed games, and saves
- [ ] **USB-07**: Windows users are guided to install libusbK/WinUSB drivers via installer or documentation

### Game Management

- [ ] **MGMT-01**: User can view all installed titles with metadata (TitleID, version, type, install location)
- [ ] **MGMT-02**: User can move installed titles between SD card and NAND
- [ ] **MGMT-03**: User can delete installed titles (base game, update, or DLC independently)
- [ ] **MGMT-04**: User can run integrity check on installed content to detect corruption

### Save Management

- [ ] **SAVE-01**: User can back up save data per game and per user account to SD card
- [ ] **SAVE-02**: User can restore save data from SD card backup without launching the game first
- [ ] **SAVE-03**: Save backup supports all save types: Account, Device, BCAT, Temporary, Cache

### File Management

- [ ] **FILE-01**: User can browse SD card contents via a built-in file manager
- [ ] **FILE-02**: User can browse NAND USER and NAND SYSTEM partitions (read-only for system)
- [ ] **FILE-03**: User can copy, move, and delete files within the file manager
- [ ] **FILE-04**: User can access Switch files wirelessly from a PC via a built-in FTP server (port 5000)
- [ ] **FILE-05**: Files >4GB are automatically split for FAT32 compatibility

### Advanced Features

- [ ] **ADV-01**: User can view all installed tickets with type labels (personalized vs common)
- [ ] **ADV-02**: User can delete individual tickets
- [ ] **ADV-03**: User can view per-game activity log (play time, launch count)
- [ ] **ADV-04**: Activity log supports per-user breakdowns

### Trust & Quality

- [x] **TRUST-01**: All source code is fully open (no obfuscation, no binary blobs)
- [x] **TRUST-02**: Reproducible builds documented so community can verify distributed binaries
- [x] **TRUST-03**: No hardcoded external network calls on app launch (user controls all outbound connections)
- [x] **TRUST-04**: App detects applet mode vs application mode and adapts memory usage accordingly

## v2 Requirements

### Enhanced UI

- **UI-01**: Borealis-based UI with controller navigation and Nintendo-style animations
- **UI-02**: Thumbnail/cover art display for installed games

### Extended Formats

- **EXT-01**: XS0 (splitNSP) container support
- **EXT-02**: Cartridge dumping (read cartridge → NSP/XCI on SD card)

### Extended Network

- **ENET-01**: Local WiFi access point creation for wireless installs without a router
- **ENET-02**: SD card counterfeit detection via write speed benchmarks

### Modding Support

- **MOD-01**: LayeredFS mod detection and management per title
- **MOD-02**: Atmosphere cheat management integration

## Out of Scope

| Feature | Reason |
|---------|--------|
| Built-in game store / piracy storefront | Not building a storefront — users configure their own servers |
| Stock firmware support | Requires CFW; out of scope by design |
| Mobile companion app (iOS/Android) | PC-first; mobile adds significant scope |
| Cloud sync for saves | High complexity, not core to install/manage use case |
| Image/text/hex file viewer | Bloat from DBI — out of scope per project focus |
| Nintendo eShop integration | Impossible without reverse engineering current auth protocols |
| Avatar/profile management | Not a content management feature |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| INST-01 | Phase 1: Foundation & Install Pipeline | Pending |
| INST-02 | Phase 1: Foundation & Install Pipeline | Pending |
| INST-03 | Phase 1: Foundation & Install Pipeline | Pending |
| INST-04 | Phase 1: Foundation & Install Pipeline | Pending |
| INST-05 | Phase 1: Foundation & Install Pipeline | Pending |
| INST-06 | Phase 1: Foundation & Install Pipeline | Pending |
| TRUST-01 | Phase 1: Foundation & Install Pipeline | Complete |
| TRUST-02 | Phase 1: Foundation & Install Pipeline | Complete |
| TRUST-03 | Phase 1: Foundation & Install Pipeline | Complete |
| TRUST-04 | Phase 1: Foundation & Install Pipeline | Complete |
| USB-01 | Phase 2: PC Companion & USB Install | Pending |
| USB-02 | Phase 2: PC Companion & USB Install | Pending |
| USB-03 | Phase 2: PC Companion & USB Install | Pending |
| USB-04 | Phase 2: PC Companion & USB Install | Pending |
| USB-07 | Phase 2: PC Companion & USB Install | Pending |
| NET-01 | Phase 3: Network Install | Pending |
| NET-02 | Phase 3: Network Install | Pending |
| NET-03 | Phase 3: Network Install | Pending |
| NET-04 | Phase 3: Network Install | Pending |
| NET-05 | Phase 3: Network Install | Pending |
| NET-06 | Phase 3: Network Install | Pending |
| NET-07 | Phase 3: Network Install | Pending |
| USB-05 | Phase 4: MTP, File & Game Management | Pending |
| USB-06 | Phase 4: MTP, File & Game Management | Pending |
| MGMT-01 | Phase 4: MTP, File & Game Management | Pending |
| MGMT-02 | Phase 4: MTP, File & Game Management | Pending |
| MGMT-03 | Phase 4: MTP, File & Game Management | Pending |
| MGMT-04 | Phase 4: MTP, File & Game Management | Pending |
| FILE-01 | Phase 4: MTP, File & Game Management | Pending |
| FILE-02 | Phase 4: MTP, File & Game Management | Pending |
| FILE-03 | Phase 4: MTP, File & Game Management | Pending |
| FILE-04 | Phase 4: MTP, File & Game Management | Pending |
| FILE-05 | Phase 4: MTP, File & Game Management | Pending |
| SAVE-01 | Phase 5: Saves, Tickets & Activity | Pending |
| SAVE-02 | Phase 5: Saves, Tickets & Activity | Pending |
| SAVE-03 | Phase 5: Saves, Tickets & Activity | Pending |
| ADV-01 | Phase 5: Saves, Tickets & Activity | Pending |
| ADV-02 | Phase 5: Saves, Tickets & Activity | Pending |
| ADV-03 | Phase 5: Saves, Tickets & Activity | Pending |
| ADV-04 | Phase 5: Saves, Tickets & Activity | Pending |

**Coverage:**
- v1 requirements: 40 total
- Mapped to phases: 40
- Unmapped: 0

---
*Requirements defined: 2026-03-25*
*Last updated: 2026-03-25 after roadmap creation*
