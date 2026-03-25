# Feature Landscape: SwitchPalace

**Domain:** Nintendo Switch homebrew content management (NRO + PC companion)
**Researched:** 2026-03-25
**Confidence:** HIGH (multiple verified sources, well-documented ecosystem)

---

## Table Stakes

Features users expect from any DBI replacement. Missing any of these = users stay on DBI.

### Game Installation

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Install NSP files from SD card | Core purpose of every installer | Low | Standard libnx NCM/NS API calls. Well-documented path. |
| Install XCI files from SD card | Physical dump format, widely distributed | Medium | Requires parsing XCI container to extract NCAs. Must handle trimmed and untrimmed XCI. |
| Install NSZ (compressed NSP) | ~30-50% smaller files, very common in community | Medium | Zstandard decompression on-the-fly during install. CPU-bound on Switch ARM cores. |
| Install XCZ (compressed XCI) | Compressed cartridge dumps | Medium | Combines XCI parsing with NSZ decompression. |
| Install split NSP/XCI files | Large games split for FAT32 SD cards | Low | Concatenate file segments before/during install. Common pattern. |
| Choose install target (SD/NAND/Auto) | DBI has this, users expect location control | Low | libnx storage ID selection. Auto = SD with NAND fallback. |
| Batch/multi-select installation | Installing one title at a time is unusable for large libraries | Low | UI selection list + sequential install queue. |
| Display install progress | Users need feedback on large (10-30GB) installs | Low | Progress callback from NCM write operations. |

### USB Transfer

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| MTP Responder mode | DBI's most popular feature. PC sees Switch as USB drive, drag-and-drop install. | High | Must implement MTP protocol on Switch side via libnx USB. DBI achieves ~20-35 MB/s. |
| PC companion USB install (like DBIbackend) | Direct USB install without MTP overhead | Medium | Custom USB protocol between NRO and PC server. Faster than MTP. |
| USB 3.0 support | Atmosphere supports USB 3.0, users expect max speed | Medium | libnx supports SuperSpeed USB. Requires proper descriptor configuration. Up to 100-120 MB/s reported with good cable. |

### Network Installation

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| HTTP server install | Install from home server over WiFi/LAN | Medium | HTTP client on Switch, browse file listings, download and install. |
| Tinfoil shop protocol (JSON index) | Community servers all use this format | Medium | Parse JSON with `files`, `directories`, `success`, `headers`, `titledb` keys. Must support HTTPS. |
| FTP server install | Some users host via FTP | Medium | FTP client implementation on Switch. |
| SFTP server install | Secure variant, mentioned in PROJECT.md requirements | Medium | SSH/SFTP client is heavier but expected. |
| Network source configuration | Users configure their own server URLs | Low | Config file and UI for managing server list. |

### Content Management

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Browse installed titles | View all installed games/DLC/updates with metadata | Low | libnx NS service enumeration. Display name, version, size, icon. |
| Delete installed titles | Basic management | Low | NS service uninstall API. |
| Move titles between SD and NAND | Users juggle limited storage | Medium | Copy NCAs to target storage, register, then remove from source. No atomic move API exists. |
| View title details (SDK version, key gen, patch info) | DBI shows this, power users expect it | Low | Read NCA headers and CNMT metadata. |
| Check for title updates/DLC availability | DBI does this | Medium | Query Nintendo CDN version API or compare against titledb. |

### Ticket Management

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| View installed tickets | Encrypted title keys, needed for troubleshooting | Low | ES service API via libnx. |
| Delete tickets | Clean up orphaned tickets | Low | ES service delete. Users expect confirmation dialog. |

### Save Management

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Backup saves to SD card | JKSV does this, DBI does this, absolute must-have | Medium | libnx FS service to mount save data, copy to SD. Must handle per-user and per-game saves. |
| Restore saves from SD backup | Paired with backup | Medium | Write save data back. Must validate save integrity. |
| Browse saves per game and per user | Navigation model users expect | Low | Enumerate save data via FS service, group by title/user. |
| Delete saves | Cleanup | Low | FS service delete with confirmation. |

### File Manager

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Browse SD card filesystem | DBI has this, basic expectation | Low | Standard filesystem operations via libnx. |
| Browse NAND partitions | Power user feature but DBI has it | Medium | Requires NAND partition mounting. Must be read-careful to avoid bricks. |
| Copy/move/delete files and folders | Basic file operations | Low | Standard FS operations. |
| Create folders | Organizational | Low | Trivial. |

### Server Modes

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| FTP server on Switch | DBI has this for wireless file access from PC | Medium | FTP server implementation on Switch. Existing open-source examples (ftpd-pro). |

### Activity & Statistics

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Per-game play time display | DBI shows this, users value it | Low | PdmQueryPlayStatisticsByApplicationId via libnx pdm service. |
| Launch count per game | Part of activity stats | Low | Same pdm service. |
| Activity chart by date | DBI has visual charts | Medium | UI rendering of time-series data. pdm provides daily breakdowns. |

---

## Differentiators

Features that set SwitchPalace apart from DBI. These are the reasons users switch.

### Performance (Primary Differentiator)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Parallel chunked HTTP downloads | DBI does sequential single-stream HTTP. Multi-stream can saturate network link. | High | PC companion splits file into chunks, Switch requests ranges in parallel. Recombine on Switch. This is the single biggest speed win for network installs. |
| Optimized USB transfer protocol | DBI's MTP tops out at ~25 MB/s, Tinfoil hits ~45 MB/s. Custom protocol can reach 100+ MB/s. | High | Design a purpose-built binary protocol between NRO and PC companion. Large transfer buffers (1MB+), minimal handshake overhead, pipelined commands. |
| Parallel NCA installation | Install multiple NCAs from a title simultaneously where I/O allows | High | Requires careful NCM placeholder management. Biggest win for titles with many small NCAs (games with lots of DLC). |
| Install queue with background downloading | Download next title while current one installs | High | Dual-buffer: download to SD temp while installing current. Requires memory management on Switch's limited 4GB RAM (homebrew gets ~300MB). |
| Resume interrupted transfers | Network drops shouldn't restart 15GB downloads | Medium | Track byte offsets per file. HTTP Range requests for resume. Store progress in temp metadata file. |

### Trust & Transparency (Core Differentiator)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Fully open source, auditable code | DBI is closed-source with XOR-obfuscated strings. Developer has threatened brick code in unauthorized translations. Users cannot verify safety. | N/A | Not a feature to build, but a property to maintain. Every line of code visible. No obfuscation ever. |
| Reproducible builds | Users can verify the NRO binary matches source code | Medium | Document build process with exact toolchain versions. CI/CD producing deterministic outputs. |
| No phone-home or telemetry | DBI's closed nature means users can't verify what data it sends | Low | Simply don't add any network calls that aren't user-initiated. Document this explicitly. |

### User Experience

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Modern borealis UI (Nintendo-style) | Familiar Switch UI language, better than DBI's basic UI | Medium | borealis is the standard homebrew UI lib. Looks like Switch system menus. Users feel at home. |
| Keyboard/controller-friendly navigation | Smooth d-pad navigation with clear focus states | Low | borealis handles this natively. |
| Search/filter installed titles | DBI has no search. Large libraries (500+ titles) need this. | Low | Simple string matching on title names. |
| Sort installed titles (name, size, play time, install date) | Better organization than DBI's flat list | Low | UI sort controls on existing data. |
| Theme support (light/dark) | borealis supports this natively | Low | Built into borealis. |

### PC Companion (Differentiator)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| High-performance Go or Rust server | DBI's dbibackend is Python with pyusb. Slow startup, high overhead. | High | Core project decision. Either language is 10-50x faster for I/O. |
| Cross-platform PC companion (Windows, Mac, Linux) | DBI's dbibackend works everywhere but is slow. SwitchPalace companion should be fast everywhere. | Medium | Go cross-compiles trivially. Rust needs per-platform CI but also works. |
| CLI and GUI modes for PC companion | Power users want CLI, casual users want GUI | Medium | CLI first (simpler), GUI later. CLI covers all features. |
| Drag-and-drop in PC companion GUI | Visual file selection for non-technical users | Medium | Desktop GUI framework (Go: Fyne/Wails; Rust: egui/Tauri). |

---

## Anti-Features

Things to deliberately NOT build. Each exclusion is intentional.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Built-in piracy storefront / "eShop" | Legal liability, ethical issues, and Tinfoil already does this. HappyFoil's built-in eShop is its most controversial feature. SwitchPalace should be a tool, not a store. | Support Tinfoil shop protocol so users can configure their own servers. Never bundle or hardcode shop URLs. |
| Game card dumping | Legal gray area, adds complexity, and dedicated tools (NXDumpTool) do this much better with more format options | Recommend NXDumpTool for dumping. Focus on installation. |
| Image/archive viewer | DBI views JPG/PNG/PSD/ZIP/RAR. This is feature bloat unrelated to content management. | Stay focused. Users have other tools for viewing images. |
| Text editor / hex viewer | DBI has these. Unrelated to core purpose. | Not a text editor. Users have ftpd + PC editors. |
| Avatar customization | DBI can set user avatars from images. Niche feature, not worth the complexity. | Omit entirely. |
| Delete parental controls | DBI offers this. Risky feature with potential for misuse. Not core to content management. | Omit. Users can manage parental controls through Atmosphere or system settings. |
| Delete user accounts | DBI offers this. Dangerous operation that should not be one tap away in an installer. | Omit. System settings handles this. |
| "Launch random game" | DBI easter egg. Not a content manager feature. | Omit. |
| Google Drive / 1Fichier protocol support | Tinfoil-specific cloud storage protocols. Adds complexity, relies on third-party API keys, and these services are primarily used for piracy distribution. | Support standard HTTP/HTTPS/FTP/SFTP only. If a user wants Google Drive, they can use rclone to mount it as a local server. |
| NTP time sync | DBI includes this. Switch system settings and Atmosphere handle time. Not an installer's job. | Omit. |
| Retroarch ROM launching | Tinfoil added this. Completely out of scope. | Omit. |
| Mobile companion app | PC-first per PROJECT.md. Mobile adds massive complexity with minimal value. | PC companion only. |

---

## Feature Dependencies

```
USB 3.0 support ──> MTP Responder (MTP benefits most from USB3 speed)
USB 3.0 support ──> PC companion USB protocol

NSZ install ──> NSP install (NSZ decompresses to NSP format during install)
XCZ install ──> XCI install + NSZ decompression

Tinfoil shop protocol ──> HTTP client (shops are served over HTTP/HTTPS)
SFTP install ──> SSH library integration

Parallel chunked downloads ──> PC companion server (server must support range requests and chunk coordination)
Parallel chunked downloads ──> HTTP client (Switch-side must handle multiple concurrent connections)

Resume interrupted transfers ──> Parallel chunked downloads (resume per-chunk)

Save restore ──> Save backup (same filesystem mounting, reverse direction)

Move titles SD<->NAND ──> Browse installed titles (need title enumeration first)

Search/filter titles ──> Browse installed titles (need the list to search)

Activity charts ──> Play time stats (chart is visualization of same data)

Install queue with background download ──> Batch installation (queue is extension of batch)

PC companion GUI ──> PC companion CLI (GUI wraps CLI functionality)
```

### Critical Path (Build Order)

```
Phase 1: NSP install from SD ──> XCI install ──> NSZ/XCZ install ──> Split file install
Phase 2: USB protocol + PC companion ──> MTP Responder
Phase 3: HTTP client ──> Tinfoil shop protocol ──> FTP/SFTP
Phase 4: Browse/delete titles ──> Move titles ──> Ticket management
Phase 5: Save backup/restore ──> File manager
Phase 6: Parallel downloads ──> Resume transfers ──> Install queue
Phase 7: Activity stats ──> FTP server mode ──> Search/sort UI polish
```

---

## Feature Complexity Notes

### Hard Features (High Risk, Budget Extra Time)

| Feature | Why Hard | Mitigation |
|---------|----------|------------|
| MTP Responder | Full MTP protocol implementation on embedded device. DBI's MTP is its most complex feature. Must handle Windows/Mac/Linux MTP quirks. | Study existing open-source MTP implementations (mtp-server-nx). Start with read-only MTP, add write later. |
| Parallel chunked HTTP downloads | Coordinating multiple HTTP streams on Switch's limited memory (~300MB for homebrew). Must handle partial failures, out-of-order chunks, and reassembly. | Start with 2-4 parallel streams. Use fixed-size ring buffers. Fall back to sequential on memory pressure. |
| Custom USB transfer protocol | Designing a fast binary protocol from scratch. Must handle error recovery, flow control, and work across Windows/Mac/Linux USB stacks. | Study Tinfoil's USB protocol (achieves 40-45 MB/s). Use large transfer buffers (512KB-1MB). Keep protocol simple: command/response with bulk transfers. |
| NSZ decompression during install | Zstandard decompression is CPU-intensive on Switch's ARM Cortex-A57 cores. Must decompress and write simultaneously without stalling. | Use streaming decompression (zstd streaming API). Double-buffer: decompress chunk N+1 while writing chunk N. May need to limit install speed to decompress rate. |
| PC companion server (Go/Rust) | Full-featured server with USB communication, file serving, and eventually GUI. | Start with CLI-only USB transfer. Add HTTP file serving. GUI is last. |

### Medium Complexity

| Feature | Notes |
|---------|-------|
| Tinfoil shop JSON parsing | Well-documented format but has many optional fields (headers, titledb, locations, certificates). Parse incrementally. |
| Save backup/restore | libnx FS save mounting is well-documented via JKSV source. Per-user save enumeration adds complexity. |
| Move titles between storage | No atomic move API. Must copy all NCAs, register in new storage, verify, then delete from old. Interruption = data loss risk. |
| FTP server on Switch | Open-source ftpd exists for reference. But implementing from scratch is medium effort. |
| Activity charts | pdm service API is straightforward. Rendering charts in borealis UI takes effort. |

### Low Complexity (Quick Wins)

| Feature | Notes |
|---------|-------|
| Browse installed titles | libnx NS enumeration is well-documented, ~50 lines of code. |
| Delete titles | Single API call with confirmation UI. |
| View/delete tickets | ES service, straightforward. |
| Install target selection | Storage ID enum, trivial. |
| Search/filter/sort titles | String matching on in-memory list. |
| Play time display | Single pdm API call per title. |
| Config file management | INI or JSON parsing, standard. |

---

## MVP Recommendation

**Prioritize (Phase 1 MVP):**
1. NSP/XCI/NSZ/XCZ installation from SD card (table stakes, proves core install pipeline works)
2. Browse and delete installed titles (immediate utility after install)
3. Basic USB install via PC companion CLI (the speed differentiator users will feel first)
4. borealis UI shell with navigation (sets the UX tone from day one)

**Phase 2 (Network + Management):**
5. HTTP install with Tinfoil shop protocol support (unlocks network installs)
6. MTP Responder (DBI's most used feature, users will demand it)
7. Save backup/restore (high-value utility feature)
8. Ticket management

**Phase 3 (Performance + Polish):**
9. Parallel chunked downloads (the big speed differentiator)
10. FTP/SFTP install sources
11. File manager
12. Activity stats and charts

**Defer:**
- FTP server mode: Nice-to-have, not core to content management
- PC companion GUI: CLI first, GUI when core is stable
- Resume interrupted transfers: Important but complex, after parallel downloads work

---

## Sources

- [DBI Switch - GameBrew](https://www.gamebrew.org/wiki/DBI_Switch)
- [DBI GitHub (rashevskyv/dbi)](https://github.com/rashevskyv/dbi)
- [DBI README](https://github.com/rashevskyv/dbi/blob/main/README.md)
- [Tinfoil Custom Index Specification](https://blawar.github.io/tinfoil/custom_index/)
- [HappyFoil GitHub](https://github.com/Tefery/HappyFoil)
- [Awoo Installer GitHub](https://github.com/Huntereb/Awoo-Installer)
- [JKSV Save Manager GitHub](https://github.com/J-D-K/JKSV)
- [NSZ Format Specification](https://nicobosshard.ch/nsz.html)
- [libnx GitHub (switchbrew/libnx)](https://github.com/switchbrew/libnx)
- [NCM Services - Nintendo Switch Brew](https://switchbrew.org/wiki/NCM_services)
- [USB Services - Nintendo Switch Brew](https://switchbrew.org/wiki/USB_services)
- [GBAtemp: USB/MTP Install Speed Discussion](https://gbatemp.net/threads/different-usb-mtp-install-speed-across-installers.637053/)
- [GBAtemp: DBI MTP Speed Discussion](https://gbatemp.net/threads/dbi-mtp-responder-getting-low-speed-after-release-527.638628/)
- [XCI vs NSP Differences](https://gbatemp.net/threads/xci-vs-nsp-what-are-the-differences.589177/)
- [mtp-server-nx - Open Source MTP for Switch](https://gbatemp.net/threads/mtp-server-nx-open-source-usb-file-transfer-for-switch.547273/)
