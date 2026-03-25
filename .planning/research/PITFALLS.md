# Pitfalls Research: SwitchPalace

**Domain:** Nintendo Switch homebrew content manager / installer
**Researched:** 2026-03-25
**Overall confidence:** MEDIUM-HIGH (combination of official SwitchBrew docs, open-source installer codebases, and extensive community reports)

---

## Critical Pitfalls (Brick/Data Loss Risk)

### CP-1: Incomplete NCA Installation Leaves Orphaned Placeholders

**What goes wrong:** If an NCA write is interrupted (crash, USB disconnect, user cancel), a placeholder file remains in NCM content storage but is never registered. Orphaned placeholders consume NAND/SD space invisibly and can confuse subsequent installation attempts for the same title.

**Why it happens:** The NCM installation flow is a multi-step process: GeneratePlaceHolderId -> CreatePlaceholder -> WritePlaceholder (repeated) -> Register. If the process dies between Create and Register, the placeholder persists. The Switch OS does not automatically clean these up.

**Consequences:** Accumulated orphaned placeholders waste storage (especially on NAND). Attempting to reinstall may fail with "already exists" errors. In extreme cases, filling NAND storage can prevent system operations.

**Prevention:**
- Implement a cleanup-on-failure handler that calls DeletePlaceholder if installation is interrupted
- On startup, scan for and offer to clean orphaned placeholders
- Never write to NAND without explicit user confirmation
- Wrap the entire install flow in a transaction-like pattern: track state, roll back on any failure

**Detection:** Installation fails mid-way; storage space lower than expected; reinstallation errors.

**Phase:** Must be addressed in the core installation engine (Phase 1/MVP). This is not a nice-to-have -- it is fundamental to safe installation.

**Confidence:** HIGH (confirmed via SwitchBrew NCM services documentation and Awoo-Installer source code)

---

### CP-2: Ticket Mismanagement Corrupts Title Rights

**What goes wrong:** Installing, deleting, or modifying tickets (rights-id entries managed by the ES service) incorrectly can make titles unlaunchable, corrupt save data associations, or cause the system to enter inconsistent states where games appear installed but cannot run.

**Why it happens:** Tickets tie cryptographic rights to title IDs. The relationship between tickets, NCAs, and CNMT (content meta) records is strict. Deleting a ticket for a title that still has installed content, or installing a duplicate ticket with different rights, creates mismatches the OS cannot resolve gracefully.

**Consequences:** Games fail to launch with "corrupted data" errors. Save data may become inaccessible if the title rights change. In worst cases, system-level titles with corrupted tickets can require a NAND restore.

**Prevention:**
- Never delete tickets without first checking if associated content still exists
- When uninstalling a title, delete content first, then ticket, then metadata -- in that order
- Provide a "ticket audit" feature that shows orphaned tickets and lets users clean them safely
- For ticket viewing/management UI, make destructive operations require confirmation with clear warnings
- Never touch system tickets (those with specific reserved title ID ranges)

**Detection:** "Corrupted data" errors on launch; titles show in home menu but fail to start.

**Phase:** Core installation engine and game management features. Ticket management UI can come later, but safe ticket handling during install/uninstall is Phase 1.

**Confidence:** HIGH (SwitchBrew ES services documentation; widely reported in GBAtemp community threads)

---

### CP-3: NAND Write Operations Without Safeguards

**What goes wrong:** Writing content directly to NAND (system storage) without proper validation can fill system storage, corrupt system partitions, or brick the console if system-reserved space is consumed.

**Why it happens:** NAND has limited capacity (~26GB usable on base model, less on OLED). Unlike SD card operations where the worst case is a reformattable card, NAND corruption can require a full NAND backup restore via Hekate. Homebrew that doesn't respect NAND free space thresholds or writes to system partitions can cause unrecoverable states.

**Consequences:** Soft brick requiring NAND restore. Loss of saves not backed up. System unable to boot to Horizon OS.

**Prevention:**
- Default all installations to SD card; require explicit opt-in for NAND
- Check free space before installation with generous margin (at least 2x the content size to account for placeholder + final copy)
- Never write to SYSTEM partition; only USER partition for game content
- Implement a "dry run" validation pass before committing writes
- Display clear warnings when NAND is the target, showing remaining free space

**Detection:** System storage critically low; boot failures after installation.

**Phase:** Phase 1 -- hardcode SD-card-first defaults from day one.

**Confidence:** HIGH (well-documented across all Switch homebrew communities)

---

### CP-4: exFAT File System Corruption

**What goes wrong:** The Switch's exFAT driver is notoriously buggy. Homebrew that performs frequent writes, especially if interrupted (crash, forced power-off), can corrupt the entire SD card file allocation table on exFAT-formatted cards. Unlike FAT32, exFAT has no backup allocation table, so corruption is unrecoverable without reformatting.

**Why it happens:** Nintendo's exFAT driver implementation has known bugs. When homebrew crashes during a write operation, the single file allocation table can be left in an inconsistent state. This is worse than FAT32 where the backup FAT provides redundancy.

**Consequences:** Complete SD card data loss. All installed games, saves (if on SD), and homebrew configuration gone.

**Prevention:**
- Strongly recommend FAT32 in documentation and in-app messaging
- Detect exFAT at runtime and show a persistent warning banner
- Minimize write operations; use buffered/batched writes rather than many small writes
- Implement write-then-verify patterns for critical data
- Handle all file operations with proper error checking and graceful failure (never leave files half-written)
- Support and correctly handle FAT32's 4GB file splitting (Atmosphere convention: files split as `filename/00`, `filename/01`, etc.)

**Detection:** App detects exFAT filesystem on mount; random file corruption; files disappearing.

**Phase:** Phase 1 -- filesystem detection and warnings from initial release. FAT32 4GB split support is mandatory for the file manager.

**Confidence:** HIGH (libnx GitHub issue #161; extensive GBAtemp documentation; official Hacks Guide Wiki recommendation)

---

## Common Development Mistakes

### DM-1: Applet Mode Memory Starvation

**What goes wrong:** When launched from the homebrew menu (Album applet), NRO applications get approximately 442MB of RAM instead of the full ~3.7GB available in title override mode. Large installations, decompression buffers, or UI rendering can exhaust this limited memory and crash.

**Why it happens:** The Switch OS allocates memory differently for applets vs full applications. The Album applet shares memory with the system. Most users first encounter homebrew through applet mode, so this is the common case, not the exception.

**Consequences:** Crashes during large file operations. NSZ decompression fails silently or crashes. UI becomes unresponsive.

**Prevention:**
- Detect applet mode at startup using `appletGetAppletType()` and adapt behavior
- In applet mode: use smaller buffers, disable parallel downloads, warn about limitations
- Display a clear indicator of current mode (DBI uses background color: blue=applet, black=full)
- Design all buffer allocations to be configurable and respect available memory
- Test extensively in applet mode, not just title override mode
- Consider providing an NSP forwarder so users can easily launch in full-memory mode

**Detection:** `appletGetAppletType()` returns `AppletType_LibraryApplet` or similar non-Application type.

**Phase:** Phase 1 -- memory-aware design must be baked in from the start, not retrofitted.

**Confidence:** HIGH (documented across DBI, Tinfoil, and Awoo; community-verified behavior)

---

### DM-2: USB Transfer Size Boundary Bug

**What goes wrong:** When sending USB transfers of approximately 0x1000000 bytes (16MB), the Switch-side USB stack silently hangs. The host side eventually times out with no indication of what went wrong. This creates mysterious, hard-to-reproduce transfer failures.

**Why it happens:** An undocumented limitation in the Switch's USB driver. The exact boundary is not precisely known (documented as "~0x1000000, exact size unknown" on SwitchBrew). Additionally, if the receive buffer is smaller than the incoming USB packet, excess data is silently discarded.

**Consequences:** USB installations randomly fail. Users report "works sometimes" behavior. Debugging is extremely difficult because there are no error codes -- just silence.

**Prevention:**
- Cap individual USB transfer sizes well below 16MB (use 8MB or smaller chunks)
- Implement transfer-level checksums or acknowledgment protocol (send chunk -> wait for ACK -> next chunk)
- Add timeout detection on both Switch and PC sides with automatic retry
- Log transfer sizes and offsets for debugging
- Ensure all USB buffers are 0x1000-byte (4KB) aligned as required by the Switch USB stack
- Flush data cache before USB transfers (required by hardware)

**Detection:** USB transfers stall with no error; host-side timeout.

**Phase:** Phase 1 for USB installation. Protocol design must account for this from the start.

**Confidence:** HIGH (documented on SwitchBrew USB services wiki page)

---

### DM-3: NCZ/NSZ Decompression Edge Cases

**What goes wrong:** NCZ (compressed NCA) decompression has several subtle requirements: the first 0x4000 bytes must remain encrypted (they are the original NCA header), section boundaries must be respected for re-encryption after decompression, and block-compressed streams have variable block sizes that must be handled correctly.

**Why it happens:** The NCZ format is not a simple "decompress and write." It requires: (1) preserving the encrypted header, (2) decompressing the body with Zstandard, (3) re-encrypting each section with the correct crypto parameters from the NCZ section header, (4) handling block compression where blocks may be stored uncompressed if they don't compress well (block size equals decompressed size = stored in plaintext).

**Consequences:** Corrupted installations that appear to succeed but crash on launch. Silent data corruption if re-encryption uses wrong section parameters. Only CNMT NCAs with non-0x4000 headers are unsupported (documented limitation).

**Prevention:**
- Study the NCZ format specification thoroughly (nicoboss/nsz GitHub repository is the authoritative source)
- Implement section-by-section processing, not monolithic decompression
- Validate decompressed size against expected NCA size before registration
- Handle the block compression edge case: if `CompressedBlockSize == decompressedBlockSize`, treat as plaintext (no decompression needed)
- Test with a variety of NSZ files: small games, large games, DLC, updates
- Implement hash verification of the final NCA against the expected ContentId (first 16 bytes of SHA-256)

**Detection:** Games install successfully but crash on launch; hash mismatch after installation.

**Phase:** Phase 2 or 3 -- NSZ support can come after basic NSP/XCI installation works. Getting basic formats right first reduces the debugging surface.

**Confidence:** MEDIUM-HIGH (NCZ spec from nicoboss/nsz repo; Awoo-Installer source code analysis)

---

### DM-4: Thread Stack and TLS Slot Exhaustion

**What goes wrong:** libnx has a limited number of TLS (Thread Local Storage) slots -- reduced from 27 to 12 in recent versions. Thread stacks must be page-aligned (4KB boundary). Creating too many threads or using libraries that consume TLS slots can silently corrupt memory or crash.

**Why it happens:** The Switch's Horizon OS has stricter resource constraints than desktop OSes. libnx wraps these constraints but doesn't always fail loudly. Thread stack allocation formula: `aligned_stack_sz = (stack_sz + sizeof(ThreadEntryArgs) + tls_sz + reent_sz + 0xFFF) & ~0xFFF`. Getting alignment wrong causes hard-to-diagnose crashes.

**Consequences:** Random crashes under load. Memory corruption that manifests far from the actual bug. "Works on my machine" syndrome because it depends on thread creation order and TLS usage patterns.

**Prevention:**
- Use a thread pool pattern with a small, fixed number of worker threads (4-6 max given the Switch's 4 cores)
- Pre-allocate thread stacks with proper alignment at startup
- Audit all libraries for TLS slot usage; budget the 12 available slots carefully
- Use the main thread's default priority (0x2C) or slightly lower for worker threads (0x2D-0x30)
- Never create threads dynamically in response to user actions; reuse from pool
- Test with thread sanitizer equivalent or add manual TLS slot counting

**Detection:** Random crashes under parallel workloads; crashes that go away when reducing thread count.

**Phase:** Phase 1 -- thread pool architecture must be designed upfront, especially since parallel downloads are a core feature.

**Confidence:** MEDIUM (libnx source code on GitHub; thread.c and thread.h; TLS slot reduction documented in changelog)

---

### DM-5: NCA Signature Validation Handling

**What goes wrong:** NCA files have cryptographic signatures. When sigpatches are missing or outdated, installations fail with "invalid NCA" errors. Installers that don't clearly communicate this fail in confusing ways.

**Why it happens:** The Switch OS validates NCA signatures against known keys. Sigpatches bypass this validation for unsigned/repackaged content. When a user's sigpatches don't match their firmware version, installation silently fails or produces cryptic error codes (e.g., 0xF601, 2001-0123).

**Consequences:** Users blame the installer for what is actually a sigpatch/firmware mismatch. Support burden increases dramatically.

**Prevention:**
- Detect sigpatch presence and version at startup; display clear status
- When NCA installation fails with specific error codes, translate to human-readable messages: "NCA signature validation failed -- your sigpatches may be outdated for firmware X.X.X"
- Log the firmware version, Atmosphere version, and sigpatch status for debugging
- Never swallow NCA-related errors silently (Awoo-Installer had a bug where ZSTD errors returned 0 instead of propagating)

**Detection:** Installation fails with error codes containing 0x236 or similar NCM/FS error codes.

**Phase:** Phase 1 -- error reporting is critical for user trust and reducing support burden.

**Confidence:** HIGH (extensively documented across GBAtemp; Awoo-Installer and Tinfoil issue trackers)

---

### DM-6: FAT32 4GB File Split Handling

**What goes wrong:** On FAT32, files cannot exceed 4GB. The Switch ecosystem convention (established by Atmosphere) is to split large files into numbered parts in a directory. Failing to handle this correctly means large games cannot be installed from SD card on FAT32-formatted cards.

**Why it happens:** Many games exceed 4GB. Atmosphere and all major homebrew tools expect the split convention: a directory with the base filename, containing files named `00`, `01`, `02`, etc. Some installers also need to handle split NSPs/XCIs when reading source files, and must produce split output when writing to FAT32.

**Consequences:** Large game installations fail. Users on FAT32 (the recommended filesystem) cannot install popular titles.

**Prevention:**
- Implement transparent split-file reading: detect directory-with-numbered-files pattern and treat as single logical file
- When writing to FAT32, automatically split output at 0xFFFF0000 bytes (just under 4GB, with margin)
- Detect filesystem type and apply splitting only when needed
- Test with files that land exactly on split boundaries
- Handle the edge case where a split part is exactly 0 bytes (can happen at boundaries)

**Detection:** Write failures with EFBIG or similar errors on large files.

**Phase:** Phase 1 -- since FAT32 is the recommended filesystem, this is table stakes.

**Confidence:** HIGH (Atmosphere convention; SwitchBrew SD Filesystem documentation; universal community recommendation)

---

## Distribution/Legal Considerations

### LC-1: DMCA and Nintendo Takedown Risk

**What goes wrong:** Nintendo has aggressively pursued DMCA takedowns against Switch-related projects on GitHub, including emulators and homebrew tools. While their focus has been on emulators (citing encryption circumvention), an open-source installer that handles encrypted NCA content could attract attention.

**Why it happens:** Nintendo claims that tools which process encrypted Switch content (even without containing keys) circumvent "technological protection measures" under 17 USC 1201. They have issued mass DMCA takedowns against dozens of GitHub repositories.

**Consequences:** Repository taken down. Development disrupted. Potential legal exposure for maintainers.

**Prevention:**
- Never include, distribute, or reference Nintendo's cryptographic keys (prod.keys, title.keys) in the repository
- Never include key derivation code that extracts keys from the console
- Rely on keys already present on the user's console (accessed via libnx system calls, which is how all homebrew operates)
- Use a license that clearly states the project's purpose (e.g., GPL or MIT)
- Host mirrors outside GitHub as contingency (GitLab, Codeberg, self-hosted)
- Keep project framing focused on "content management" not "game installation" in public-facing materials
- Do not include or link to sources for pirated content; the "users configure their own servers" model is correct

**Detection:** DMCA notice from GitHub.

**Phase:** Pre-release -- repository setup and legal posture must be correct before any public code is pushed.

**Confidence:** MEDIUM (Nintendo's DMCA focus has been on emulators, not homebrew tools that run on the console itself; Atmosphere, DBI, and Tinfoil have operated without GitHub takedowns. However, the legal landscape is unpredictable.)

---

### LC-2: Key Material and Encryption Boundary

**What goes wrong:** An open-source installer must handle encrypted NCAs but must never expose how encryption keys are derived. Including key generation, key files, or prod.keys in the repo is both a legal risk and a trust violation.

**Why it happens:** NCA processing requires decryption keys for header parsing and signature validation. These keys exist on every Switch console and are accessible via Atmosphere's key derivation. The temptation is to hardcode or bundle them for convenience.

**Consequences:** Legal takedown. Community trust destroyed. Project becomes a piracy tool rather than a management tool.

**Prevention:**
- Use `splCryptoGenerateAesKey` and related SPL services for all key operations -- keys never leave the secure processor
- For header parsing that requires key material, use the console's own key store (accessible via `splCrypto` services on Atmosphere)
- Never log, export, or display key material
- Add CI checks that scan for common key patterns and block commits containing them
- Document clearly in README that the project does not contain or distribute any cryptographic keys

**Detection:** Code review; CI scanning for hex patterns matching known key sizes.

**Phase:** Phase 1 -- encryption handling architecture must be correct from the first line of code.

**Confidence:** HIGH (this is a well-understood boundary in the Switch homebrew ecosystem)

---

### LC-3: The "DBI Problem" -- Closed Source Trust Erosion

**What goes wrong:** DBI's developer threatened to brick consoles, used XOR string obfuscation, and made the project closed-source. This created a massive trust deficit in the community. SwitchPalace must not repeat any of these patterns.

**Why it happens:** DBI's developer felt mistreated by English-speaking users and retaliated with obfuscation and threats. While reverse engineering confirmed no brick code existed, the damage to trust was done.

**Consequences for SwitchPalace (if repeated):** Community abandonment. Fork wars. Loss of the core value proposition.

**Prevention:**
- All code fully open and auditable at all times -- no exceptions
- No obfuscation of any kind, even for debug/diagnostic strings
- Reproducible builds so users can verify the NRO matches the source
- Clear, professional communication in all community interactions
- Never joke about or reference brick capabilities, even sarcastically
- Establish community governance (multiple maintainers with commit access) to prevent single-point-of-failure

**Detection:** Community sentiment monitoring; code review processes.

**Phase:** Ongoing from project inception. This is a cultural/governance decision, not a technical one.

**Confidence:** HIGH (extensively documented DBI community incidents on GBAtemp)

---

## Moderate Pitfalls

### MP-1: Network Protocol Compatibility Drift

**What goes wrong:** The Tinfoil shop protocol (URLList/JSON format) is not formally specified. It has evolved over time through Tinfoil's closed-source updates. Implementing against outdated documentation or assumptions leads to incompatibility with popular community servers.

**Prevention:**
- Reverse-engineer the current Tinfoil protocol by testing against live community servers
- Implement protocol version detection and graceful fallback
- Support both HTTP basic auth and token-based auth schemes
- Handle URL encoding edge cases (spaces, unicode characters in filenames)
- Test against multiple shop server implementations (nut, tits-pro, custom)

**Phase:** Phase 2 -- network installation feature.

**Confidence:** MEDIUM (protocol is not formally documented; must be reverse-engineered from behavior)

---

### MP-2: MTP Protocol Complexity

**What goes wrong:** MTP (Media Transfer Protocol) over USB is complex to implement correctly. Windows, macOS, and Linux each have different MTP client behaviors. DBI's MTP mode is one of its most valued features, and reimplementing it with full OS compatibility is non-trivial.

**Prevention:**
- Start with a simpler custom USB protocol for initial release (like DBI's dbibackend)
- Add MTP support as a later feature after the custom protocol is proven
- Test MTP on Windows 10, Windows 11, macOS, and at least Ubuntu/Fedora
- Handle MTP session management carefully (device reset, reconnect)
- Be aware that Windows Explorer's MTP client has aggressive caching that can show stale file listings

**Phase:** Phase 3 or later -- MTP is a convenience feature, not core functionality.

**Confidence:** MEDIUM (MTP complexity is well-known; Switch-specific MTP behavior observed from DBI community reports)

---

### MP-3: UI Framework Memory Overhead

**What goes wrong:** Using borealis (the Nintendo-style UI framework for Switch homebrew) or LVGL without careful memory management can consume too much RAM, especially in applet mode. Complex UI screens with many elements, textures, or animations can push memory usage past safe limits.

**Prevention:**
- Profile memory usage of UI framework early in development
- Implement lazy loading for lists (game libraries can have hundreds of entries)
- Use texture atlases rather than individual images
- Free UI resources aggressively when screens are not visible
- Set a memory budget for UI (e.g., 50MB) and enforce it
- Test with large game libraries (500+ titles) to catch memory leaks

**Phase:** Phase 1 -- UI framework selection and memory budgeting.

**Confidence:** MEDIUM (general embedded UI knowledge; community reports of applet mode memory issues)

---

### MP-4: Save Backup/Restore Account Binding

**What goes wrong:** Save data on the Switch is bound to user accounts. Restoring saves to a different account or after account changes can make them unreadable. Overwriting saves without proper backup is irreversible.

**Prevention:**
- Always create a backup before any restore operation (backup-before-restore pattern)
- Store account UID alongside save data in backup files
- Warn when restoring to a different account than the original
- Implement save backup format versioning from day one
- Never offer batch save operations without individual confirmation

**Phase:** Phase 2-3 -- save management feature.

**Confidence:** MEDIUM (community reports; Checkpoint homebrew patterns)

---

## Minor Pitfalls

### mP-1: devkitPro Toolchain Version Sensitivity

**What goes wrong:** libnx, devkitA64, and system headers can have breaking changes between versions. Building against the wrong version produces NROs that crash on newer firmware or exhibit subtle bugs.

**Prevention:**
- Pin exact devkitPro/libnx versions in build configuration
- Document required toolchain versions in README and CI
- Test against latest Atmosphere release before each SwitchPalace release
- Monitor the libnx changelog for breaking changes (e.g., TLS slot reduction, ABI changes)

**Phase:** Phase 1 -- build system setup.

**Confidence:** HIGH (libnx changelog documents numerous breaking changes)

---

### mP-2: Progress Reporting Accuracy

**What goes wrong:** Inaccurate progress bars (jumping from 10% to 90%, going backwards, or stalling at 99%) destroy user confidence. Users cancel installations that are actually proceeding normally.

**Prevention:**
- Track bytes written vs total bytes for accurate percentage
- For NSZ, estimate decompressed size from the NCZ header (it contains section sizes)
- Update progress UI at a fixed interval (e.g., every 100ms) not every write
- Show transfer speed alongside percentage
- Display "finalizing..." state for the registration step (which can take seconds)

**Phase:** Phase 1 -- accurate progress is essential for user trust.

**Confidence:** HIGH (universal UX principle; reported frustration with multiple installers)

---

### mP-3: Error Code Translation

**What goes wrong:** libnx and Horizon OS return numeric error codes (e.g., 0x236E02, 2001-0123) that are meaningless to users. Failing to translate these into human-readable messages makes the app unusable for non-technical users.

**Prevention:**
- Build a mapping of common NCM, FS, NS, and ES error codes to plain-English messages
- Include the raw error code alongside the human message for advanced users
- Log full error context (operation, file being processed, offset, error code) for debugging
- Reference switchbrew.org/wiki/Error_codes for the authoritative list

**Phase:** Phase 1 -- error handling framework.

**Confidence:** HIGH (universal across all Switch homebrew tools)

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Core installation engine | CP-1: Orphaned placeholders | Transaction pattern with rollback |
| Core installation engine | CP-2: Ticket mismanagement | Strict install/uninstall ordering |
| Core installation engine | CP-3: NAND safety | SD-first defaults, space checks |
| USB transfer protocol | DM-2: 16MB boundary bug | Cap chunk size at 8MB |
| File system operations | CP-4: exFAT corruption | Detect and warn; minimize writes |
| File system operations | DM-6: FAT32 splitting | Transparent split-file handling |
| NSZ/XCZ support | DM-3: Decompression edge cases | Section-by-section processing |
| Parallel downloads | DM-4: Thread exhaustion | Fixed thread pool, TLS budgeting |
| Network shops | MP-1: Protocol drift | Test against live servers |
| Save management | MP-4: Account binding | Backup-before-restore pattern |
| Public release | LC-1: DMCA risk | No keys in repo; clean framing |
| Public release | LC-3: Trust | Full transparency; reproducible builds |

---

## Sources

- [SwitchBrew NCM Services](https://switchbrew.org/wiki/NCM_services) -- NCM API documentation, placeholder management
- [SwitchBrew USB Services](https://switchbrew.org/wiki/USB_services) -- USB transfer limitations, buffer alignment, 16MB hang bug
- [SwitchBrew NCA Format](https://switchbrew.org/wiki/NCA) -- NCA structure, encryption, content IDs
- [SwitchBrew SD Filesystem](https://switchbrew.org/wiki/SD_Filesystem) -- FAT32 split file conventions
- [nicoboss/nsz GitHub](https://github.com/nicoboss/nsz) -- NCZ/NSZ/XCZ format specification
- [Huntereb/Awoo-Installer](https://github.com/Huntereb/Awoo-Installer) -- Open-source installer reference implementation
- [switchbrew/libnx](https://github.com/switchbrew/libnx) -- Thread management, TLS slots, memory alignment
- [libnx exFAT issue #161](https://github.com/switchbrew/libnx/issues/161) -- exFAT corruption documentation
- [DBI GitHub (rashevskyv/dbi)](https://github.com/rashevskyv/dbi) -- DBI documentation and known issues
- [GBAtemp: DBI brick threat discussion](https://gbatemp.net/threads/has-anyone-actually-been-bricked-by-dbi.673434/) -- DBI trust issues
- [GBAtemp: exFAT still problematic](https://gbatemp.net/threads/exfat-still-problematic-for-sd-cards.639305/) -- exFAT community experience
- [GBAtemp: Applet mode vs title override](https://gbatemp.net/threads/what-in-the-nine-hells-is-title-override.605707/) -- Memory mode documentation
- [Hacks Guide Wiki: Switch exFAT](https://wiki.hacks.guide/wiki/Switch:ExFAT) -- Official recommendation against exFAT
- [Nintendo DMCA takedowns (GBAtemp)](https://gbatemp.net/threads/nintendo-is-yet-again-issuing-dmca-notices-to-switch-emulators-on-github.679723/) -- Legal landscape
