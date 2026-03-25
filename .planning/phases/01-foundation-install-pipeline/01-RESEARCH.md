# Phase 1: Foundation & Install Pipeline - Research

**Researched:** 2026-03-25
**Domain:** Nintendo Switch homebrew NRO development — NCA installation pipeline, borealis UI, trust infrastructure
**Confidence:** HIGH (core patterns verified against multiple open-source installers and official libnx docs)

## Summary

This phase builds a greenfield Nintendo Switch NRO application using devkitPro/devkitA64 + libnx + borealis (xfangfang fork) that installs NSP, XCI, NSZ, and XCZ game files from SD card to the Switch's SD or NAND storage. The core technical challenge is the NCA streaming installation pipeline: parsing container formats (PFS0 for NSP, HFS0 for XCI), decompressing NCZ blocks (Zstandard) for NSZ/XCZ, writing NCAs through the NCM placeholder system, registering content metadata, and importing tickets — all with atomic rollback on failure.

The established pattern for Switch installers (Awoo-Installer, Tinfoil, Goldleaf) is well-documented in open source. The pipeline follows a consistent sequence: parse container -> read CNMT NCA -> create NCM placeholders -> stream NCA data to placeholders -> register content -> commit metadata database -> push application record. Rollback means deleting all placeholders created during the failed install.

The borealis UI framework (xfangfang fork) provides Nintendo-style UI components with XML layout support, controller navigation, and C++17 APIs. It supports applet_frame, tab_frame, button, label, header, image, scrolling_frame, sidebar, and rectangle views. Progress bars and custom dialogs will need to be built as custom views or sourced from example projects (wiliwili, Moonlight-Switch) that extend borealis.

**Primary recommendation:** Model the install pipeline directly on Awoo-Installer's architecture (Install base class -> NSP/XCI subclasses -> NcaWriter with NCZ support), adapt it to borealis UI, add explicit placeholder cleanup for rollback, and use devkitPro's official Docker image for reproducible builds.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- Recursive scan of all SD card subdirectories — show all .nsp, .xci, .nsz, .xcz files in a flat list
- Files sorted alphabetically by filename
- Multi-select enabled: user can queue multiple files for batch install in one go
- User chooses SD card or NAND per batch, after selecting files and before install begins
- Prompt shown once for the entire queue (not per file)
- If a title is already installed at the destination: fail with a clear message telling the user to delete the existing version first
- No silent overwrites — explicit user action required before reinstalling
- Mandatory hash verification for every install — cannot be skipped
- Hash verification result shown inline during the progress view (checkmark or X per file as it completes)
- Also shown on the post-install summary screen
- Overall batch progress bar (%) + current filename shown below the bar
- Transfer speed (MB/s) and ETA displayed
- Cancel button available during install — pressing it shows a confirmation dialog warning that the partial install will be rolled back (NCM placeholders removed, INST-06 rollback)
- After batch completes: summary screen listing each file with pass/fail result
- User dismisses with B/confirm to return to the file list
- App detects and displays a persistent "APPLET" badge in a corner of the main screen when running as an applet
- In applet mode: NSZ and XCZ installs are disabled (decompression is memory-intensive and unsafe in applet memory budget)
- Disabled items shown grayed with a label indicating they require application mode
- NSP and XCI installs remain available in applet mode

### Claude's Discretion
- Heap allocation strategy: conservative caps in applet mode, generous in application mode — Claude picks specific values
- NSZ/XCZ decompression buffer sizing and Zstandard library integration
- borealis layout details, spacing, and animation choices
- NCM placeholder management implementation details for rollback

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INST-01 | Install NSP files (base games, DLC, updates) to SD or NAND | PFS0 container parsing + NCA streaming pipeline via NCM placeholders; ticket/cert import via ES service |
| INST-02 | Install XCI files (cartridge dumps) to SD or NAND | HFS0 secure partition parsing + same NCA pipeline; skip update/normal partitions |
| INST-03 | Install NSZ and XCZ files (compressed formats) with on-the-fly decompression | NCZ format: first 0x4000 bytes uncompressed NCA header, then NCZSECTN sections + zstd stream; use switch-zstd package |
| INST-04 | Choose install destination (SD card vs NAND) before each install | NcmStorageId_SdCard vs NcmStorageId_BuiltInUser passed to ncmOpenContentStorage/ncmOpenContentMetaDatabase |
| INST-05 | Verify file integrity via hash check during installation | NCA header RSA-2048 PSS signature verification + SHA-256 content hash per NCA filename convention |
| INST-06 | Atomic installation — interrupted installs rolled back, no orphaned NCM placeholders | Track all created placeholder IDs; on failure/cancel, call ncmContentStorageDeletePlaceHolder for each; use ncmContentStorageCleanupAllPlaceHolder as safety net |
| TRUST-01 | All source code fully open (no obfuscation, no binary blobs) | Greenfield project — enforce from day one; no closed-source dependencies |
| TRUST-02 | Reproducible builds documented so community can verify distributed binaries | devkitpro/devkita64 Docker image with pinned package versions; Makefile + documented build steps |
| TRUST-03 | No hardcoded external network calls on app launch | Audit all initialization code; no fetch/curl/HTTP at startup; user controls all outbound |
| TRUST-04 | App detects applet mode vs application mode and adapts memory usage accordingly | appletGetAppletType() returns AppletType enum; adjust __nx_heap_size and disable NSZ/XCZ in applet mode |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| devkitA64 | latest via dkp-pacman | ARM64 cross-compiler for Switch | Only supported toolchain for Switch homebrew |
| libnx | latest via switch-dev meta-package | Switch system service wrappers (NCM, NS, ES, applet, fs) | Official homebrew SDK by switchbrew |
| borealis (xfangfang fork) | HEAD of main branch | Nintendo-style UI framework with XML layouts | Most actively maintained fork; supports Switch + PC; C++17 |
| switch-zstd | latest via dkp-pacman | Zstandard compression/decompression for NCZ | Required for NSZ/XCZ decompression; available as devkitPro portlib |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| switch-zlib | latest via dkp-pacman | General compression utilities | May be needed for some container operations |
| switch-mbedtls or libnx crypto | bundled with libnx | AES-XTS/CTR decryption, RSA-2048 verification | NCA header decryption and signature verification |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| borealis (xfangfang) | natinusala/borealis (original) | Original is unmaintained; xfangfang has multi-platform support and active development |
| borealis | LVGL | LVGL is lower-level, no built-in Switch styling, more work for Nintendo-like UI |
| Custom PFS0/HFS0 parser | Reuse Awoo-Installer's parsers | Awoo's parsers are tightly coupled; better to write clean implementations following the same format specs |

**Installation:**
```bash
# On the build host (or in Docker):
sudo dkp-pacman -S switch-dev switch-zstd
# borealis is a git submodule, not a pacman package
git submodule add https://github.com/xfangfang/borealis.git lib/borealis
```

## Architecture Patterns

### Recommended Project Structure
```
SwitchPalace/
├── Makefile                    # devkitPro standard Makefile
├── Dockerfile                  # Reproducible build environment
├── lib/
│   └── borealis/               # git submodule
├── include/
│   ├── install/
│   │   ├── install.hpp         # Base install class (abstract)
│   │   ├── nsp.hpp             # NSP-specific container logic
│   │   ├── xci.hpp             # XCI-specific container logic
│   │   ├── nca_writer.hpp      # NCA streaming writer with NCZ support
│   │   └── pfs0.hpp / hfs0.hpp # Container format parsers
│   ├── nx/
│   │   ├── content_mgmt.hpp    # NCM service wrapper
│   │   ├── ticket.hpp          # ES ticket import wrapper
│   │   └── applet.hpp          # Applet mode detection
│   ├── ui/
│   │   ├── file_browser.hpp    # SD card file list view
│   │   ├── install_view.hpp    # Progress + status view
│   │   └── summary_view.hpp    # Post-install results
│   └── util/
│       ├── crypto.hpp          # AES-XTS, RSA-2048, SHA-256
│       └── file_util.hpp       # SD card scanning, path handling
├── source/
│   ├── main.cpp                # Entry point, borealis init, applet detection
│   ├── install/                # Install pipeline implementation
│   ├── nx/                     # System service wrappers
│   ├── ui/                     # borealis view implementations
│   └── util/                   # Utilities
├── resources/
│   └── xml/                    # borealis XML layouts
└── romfs/                      # Embedded resources for NRO
```

### Pattern 1: NCA Streaming Installation Pipeline
**What:** The core install flow that all formats share — stream NCA data through placeholders to registered content.
**When to use:** Every single install operation.
**Flow:**
```
1. Parse container (PFS0 for NSP, HFS0 secure partition for XCI)
2. Find and read CNMT NCA → extract content records (NcmContentInfo list)
3. For each NCA in content records:
   a. Generate placeholder ID: ncmContentStorageGeneratePlaceHolderId()
   b. Create placeholder: ncmContentStorageCreatePlaceHolder(cs, &contentId, &placeholderId, size)
   c. Track placeholder in rollback list
   d. Stream data in chunks: ncmContentStorageWritePlaceHolder(cs, &placeholderId, offset, data, dataSize)
   e. For NCZ: decompress zstd stream on-the-fly, re-encrypt sections, then write
   f. Verify hash/signature
   g. Register: ncmContentStorageRegister(cs, &contentId, &placeholderId)
4. Commit metadata: ncmContentMetaDatabaseSet() + ncmContentMetaDatabaseCommit()
5. Register app: nsPushApplicationRecord()
6. Import tickets: esImportTicket()
```

### Pattern 2: Atomic Rollback via Placeholder Tracking
**What:** Maintain a vector of all created placeholder IDs during an install. On any failure or cancellation, iterate and delete all of them.
**When to use:** Always — this is the INST-06 safety mechanism.
**Example:**
```cpp
class InstallContext {
    NcmContentStorage contentStorage;
    std::vector<NcmPlaceHolderId> createdPlaceholders;

    void createPlaceholder(const NcmContentId& contentId, s64 size) {
        NcmPlaceHolderId phId;
        ncmContentStorageGeneratePlaceHolderId(&contentStorage, &phId);
        Result rc = ncmContentStorageCreatePlaceHolder(&contentStorage, &contentId, &phId, size);
        if (R_FAILED(rc)) throw InstallError(rc);
        createdPlaceholders.push_back(phId);
    }

    void rollback() {
        for (auto& phId : createdPlaceholders) {
            ncmContentStorageDeletePlaceHolder(&contentStorage, &phId);
        }
        createdPlaceholders.clear();
    }
};
```

### Pattern 3: Applet Mode Adaptation
**What:** Detect execution context at startup and configure heap + feature availability accordingly.
**When to use:** main.cpp initialization.
**Example:**
```cpp
// Override heap size based on mode
// Application mode: up to ~3.2GB available, request generous heap
// Applet mode: ~40-80MB available, request conservative heap
extern u32 __nx_heap_size;

void configureForMode() {
    AppletType type = appletGetAppletType();
    bool isAppletMode = (type != AppletType_Application &&
                         type != AppletType_SystemApplication);
    if (isAppletMode) {
        // Conservative: leave room for system
        // NSZ/XCZ disabled in UI
    }
}
```

### Anti-Patterns to Avoid
- **Reading entire files into memory:** NCA files can be gigabytes. Always stream in chunks (e.g., 1-8MB buffers). Never load an entire NCA into RAM.
- **Ignoring placeholder cleanup:** Awoo-Installer's source shows limited rollback. This project MUST track and clean up all placeholders on failure.
- **Hardcoding storage paths:** Use NCM service APIs to get content paths, never construct them manually.
- **Blocking the UI thread:** Install operations must run in a background thread with progress callbacks to the borealis UI.
- **Forgetting CNMT NCA:** The CNMT (content meta) NCA must be installed first — it tells the system what other NCAs to expect. Installing data NCAs without CNMT leaves the system in an inconsistent state.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Zstandard decompression | Custom decompressor | switch-zstd (libzstd) | ZSTD_decompressStream is battle-tested; NCZ block format requires correct framing |
| AES-XTS decryption | Custom AES implementation | libnx hw-accelerated crypto or switch-mbedtls | NCA headers use non-standard AES-XTS with reversed tweak endianness |
| RSA-2048 PSS verification | Custom RSA | libnx crypto / mbedtls | NCA signature verification is security-critical |
| Container format parsing (PFS0/HFS0) | Generic archive library | Purpose-built parsers following switchbrew specs | Simple formats (~50 lines each) but must handle exact field layouts |
| NCM/NS/ES service calls | Direct IPC | libnx wrapper functions (ncm*, ns*, es*) | libnx abstracts IPC versioning across firmware versions |
| UI framework | Custom nanovg UI | borealis (xfangfang) | Months of work to replicate Nintendo-style UI; borealis does it out of box |
| SHA-256 hashing | Custom hash | libnx or switch-mbedtls | Standard library, no reason to reimplement |

**Key insight:** The Switch homebrew ecosystem has well-established patterns for installation. The innovation here is in UI quality, rollback safety, and openness — not in the low-level install mechanics.

## Common Pitfalls

### Pitfall 1: Applet Mode Memory Exhaustion
**What goes wrong:** App crashes or hangs when decompressing NSZ/XCZ in applet mode due to limited heap (~40-80MB).
**Why it happens:** Zstandard decompression buffers + NCA write buffers can easily exceed applet memory budget. The ZSTD decompression context alone can use significant memory.
**How to avoid:** Detect applet mode at startup. Disable NSZ/XCZ installs entirely in applet mode (per user decision). For NSP/XCI in applet mode, use smaller streaming buffers (512KB-1MB instead of 4-8MB).
**Warning signs:** svcSetHeapSize fails, malloc returns null, app crashes silently.

### Pitfall 2: Orphaned Placeholders After Crash
**What goes wrong:** If the app crashes (not just cancellation), placeholders remain on the filesystem consuming space and potentially blocking future installs.
**Why it happens:** C++ destructors don't run on hard crash (power loss, unhandled exception in system service).
**How to avoid:** On startup, call ncmContentStorageListPlaceHolder() and ncmContentStorageCleanupAllPlaceHolder() to clean up any stale placeholders from previous failed runs. This is the "safety net" beyond per-install tracking.
**Warning signs:** "Not enough space" errors when space should be available; failed reinstall attempts.

### Pitfall 3: NCA Header Encryption Quirks
**What goes wrong:** NCA header decryption produces garbage, failing magic number validation ("NCA3").
**Why it happens:** NCA uses AES-XTS with a non-standard tweak where endianness is reversed compared to the IEEE standard. Most AES-XTS implementations assume standard tweak handling.
**How to avoid:** Use the exact same AES-XTS implementation as other Switch tools (Awoo-Installer's Crypto.cpp, or libnx's built-in hw crypto). Test against known-good NCA files.
**Warning signs:** Magic value check fails on every NCA.

### Pitfall 4: XCI Secure Partition Offset Calculation
**What goes wrong:** Reading NCA data from XCI files produces corrupted output.
**Why it happens:** The XCI root HFS0 is at offset 0xF000 (not 0x0 or 0x10000). The secure partition offset must be calculated from the root HFS0 file entry, adding the header size and data offset. Off-by-one in offset calculation is common.
**How to avoid:** Follow the exact offset calculation: `secureOffset = rootHfs0Offset + sizeof(Hfs0Header) + entry->dataOffset`. Validate by checking the HFS0 magic ("HFS0") at the computed offset.
**Warning signs:** HFS0 magic check fails for the secure partition.

### Pitfall 5: NCZ Section Re-encryption After Decompression
**What goes wrong:** Installed NCZ-sourced NCAs fail to launch — the system rejects them.
**Why it happens:** NCZ files store decompressed NCA data that must be re-encrypted per the section entries in the NCZ header before writing to the placeholder. Each section has its own crypto type, key, and counter. Forgetting re-encryption or using wrong section boundaries produces invalid NCAs.
**How to avoid:** Parse all NCZSECTN entries. After decompressing each chunk, apply the correct AES-CTR encryption for the section that chunk belongs to before writing to the NCM placeholder.
**Warning signs:** Install completes but game won't launch; system reports corrupted data.

### Pitfall 6: borealis Build Configuration
**What goes wrong:** Build fails with obscure template errors or RTTI failures.
**Why it happens:** borealis requires C++17 with RTTI and exceptions enabled. The default devkitPro Makefile template includes `-fno-rtti -fno-exceptions`.
**How to avoid:** Remove `-fno-rtti` and `-fno-exceptions` from CXXFLAGS. Ensure `-std=c++17` or `-std=gnu++17` is set.
**Warning signs:** Compile errors in borealis headers mentioning `typeid`, `dynamic_cast`, or `throw`.

### Pitfall 7: Already-Installed Title Detection
**What goes wrong:** Overwriting an existing install corrupts the content database or creates duplicate entries.
**Why it happens:** NCM doesn't prevent creating a placeholder with a content ID that's already registered. If you register again, you may get database inconsistencies.
**How to avoid:** Before install, query ncmContentStorageHas() for each NCA content ID and check ncmContentMetaDatabaseGet() for existing CNMT records. If the title exists at the target storage, block the install with a user-facing error message (per user decision: "delete existing version first").
**Warning signs:** Duplicate entries in game list, launch failures, wasted storage.

## Code Examples

### PFS0 (NSP Container) Header Parsing
```cpp
// Source: switchbrew.org/wiki/NCA + Awoo-Installer pattern
struct PFS0Header {
    char magic[4];     // "PFS0"
    u32 fileCount;
    u32 stringTableSize;
    u32 reserved;
};

struct PFS0FileEntry {
    u64 dataOffset;
    u64 dataSize;
    u32 stringTableOffset;
    u32 reserved;
};

// Read header, validate magic, then read fileCount entries + string table
// Entry names are in the string table at stringTableOffset
// Actual file data starts after: sizeof(PFS0Header) + fileCount * sizeof(PFS0FileEntry) + stringTableSize
```

### HFS0 (XCI Partition) Header Parsing
```cpp
// Source: switchbrew.org/wiki/XCI
struct HFS0Header {
    char magic[4];     // "HFS0"
    u32 fileCount;
    u32 stringTableSize;
    u32 reserved;
};

struct HFS0FileEntry {
    u64 dataOffset;
    u64 dataSize;
    u32 stringTableOffset;
    u32 hashedSize;
    u64 reserved;
    u8  hash[0x20];    // SHA-256
};

// XCI root HFS0 starts at offset 0xF000
// Parse root to find "secure" partition entry
// Parse secure partition HFS0 to enumerate NCAs
```

### NCZ Decompression Pattern
```cpp
// Source: nicoboss/nsz format specification
#define NCZ_SECTION_MAGIC 0x4E43535A4543544E  // "NCZSECTN" as u64
#define NCZ_BLOCK_MAGIC   0x4E435A424C4F434B  // "NCZBLOCK" as u64

struct NczSectionHeader {
    u64 magic;          // NCZ_SECTION_MAGIC
    u64 sectionCount;
};

struct NczSectionEntry {
    u64 offset;
    u64 size;
    u64 cryptoType;
    u64 padding;
    u8  cryptoKey[16];
    u8  cryptoCounter[16];
};

// First 0x4000 bytes: unchanged NCA header (write directly)
// At 0x4000: read NczSectionHeader + entries
// After sections: optional NCZBLOCK header for block compression
// Then: zstd stream — decompress, re-encrypt per section, write to placeholder
```

### Applet Mode Detection
```cpp
// Source: libnx applet.h
#include <switch.h>

bool isAppletMode() {
    AppletType type = appletGetAppletType();
    return (type != AppletType_Application &&
            type != AppletType_SystemApplication);
}

// In main():
if (isAppletMode()) {
    // Show "APPLET" badge
    // Gray out .nsz and .xcz files in file list
    // Use smaller I/O buffers
}
```

### NCM Content Storage Usage
```cpp
// Source: libnx ncm.h documentation
NcmContentStorage contentStorage;
Result rc = ncmOpenContentStorage(&contentStorage, NcmStorageId_SdCard);
// or NcmStorageId_BuiltInUser for NAND

// Check free space
s64 freeSpace;
ncmContentStorageGetFreeSpaceSize(&contentStorage, &freeSpace);

// Create placeholder
NcmPlaceHolderId phId;
ncmContentStorageGeneratePlaceHolderId(&contentStorage, &phId);
ncmContentStorageCreatePlaceHolder(&contentStorage, &contentId, &phId, ncaSize);

// Stream write in chunks
for (u64 offset = 0; offset < ncaSize; offset += chunkSize) {
    // read chunk from source file
    ncmContentStorageWritePlaceHolder(&contentStorage, &phId, offset, buffer, bytesRead);
}

// Finalize
ncmContentStorageRegister(&contentStorage, &contentId, &phId);

// Cleanup on failure
ncmContentStorageDeletePlaceHolder(&contentStorage, &phId);
ncmContentStorageClose(&contentStorage);
```

### Content Meta Database Registration
```cpp
// Source: libnx ncm.h + Awoo-Installer pattern
NcmContentMetaDatabase metaDb;
ncmOpenContentMetaDatabase(&metaDb, storageId);

// Set metadata (serialized CNMT data)
ncmContentMetaDatabaseSet(&metaDb, &metaKey, contentMetaData, contentMetaSize);
ncmContentMetaDatabaseCommit(&metaDb);
ncmContentMetaDatabaseClose(&metaDb);

// Register application record
NsApplicationRecord appRecord = {};
appRecord.application_id = baseTitleId;
appRecord.type = NsApplicationRecordType_Installed;
nsPushApplicationRecord(baseTitleId, NsApplicationRecordType_Installed, &storageRecord, recordCount);
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| natinusala/borealis | xfangfang/borealis | ~2023 | Original unmaintained; xfangfang has multi-platform, active dev |
| Custom AES in homebrew | libnx hw-accelerated crypto | libnx 2.0+ | Faster, more reliable NCA processing |
| Manual IPC for NCM | libnx ncm wrapper functions | libnx 3.0+ | Clean C API, handles firmware version differences |
| Awoo-Installer (last active) | No clear successor | ~2021 | Awoo abandoned; Tinfoil is closed-source; gap SwitchPalace fills |
| devkitPro manual install | Docker images + dkp-pacman | ~2020+ | Reproducible builds via container |

**Deprecated/outdated:**
- Awoo-Installer: Abandoned since ~2021, but its architecture remains the best open-source reference for NCA installation
- natinusala/borealis: Original repo unmaintained; use xfangfang fork
- Goldleaf: Another open-source installer but less feature-complete for NSZ/XCZ

## Open Questions

1. **Exact heap budget in applet mode**
   - What we know: Applet mode has significantly less memory than application mode (~40-80MB vs ~3.2GB cited in community discussions, but no official figure)
   - What's unclear: Exact available heap varies by system firmware and what other applets are loaded
   - Recommendation: Use __nx_heap_size = 0 (auto-detect available) and then query actual allocation. Set safe buffer sizes based on what was actually granted. Test on real hardware.

2. **borealis progress bar widget**
   - What we know: borealis provides label, button, image, header, scrolling_frame, tab_frame, sidebar, applet_frame, rectangle views
   - What's unclear: No built-in progress bar widget identified in the views directory. Projects like wiliwili likely implement custom progress views.
   - Recommendation: Build a custom ProgressBar view extending brls::Box or brls::Rectangle. Check wiliwili source for reference implementation.

3. **NCA header key source**
   - What we know: NCA header decryption requires AES-XTS keys. On a real Switch with Atmosphere, these keys are available from the key store.
   - What's unclear: Whether libnx provides direct key access or if the app must read from a key file
   - Recommendation: Study how Awoo-Installer obtains header keys. On Atmosphere, keys are typically derived from the system. The NCM service handles most encryption transparently when using the placeholder write API — the NCA data written to placeholders may not need manual decryption for installation (NCM handles it internally for registered content).

4. **Docker image version pinning for reproducibility**
   - What we know: devkitpro/devkita64 Docker image exists; devkitNix project provides Nix-based reproducible builds
   - What's unclear: Whether devkitPro Docker images have stable tags for version pinning (they don't use semver)
   - Recommendation: Pin Docker image by SHA digest in Dockerfile. Document exact dkp-pacman package versions in build instructions. Consider using devkitNix as an alternative for stronger reproducibility guarantees.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Manual testing on Switch hardware + basic host-side unit tests |
| Config file | None — Wave 0 will establish |
| Quick run command | `make` (build succeeds = basic validation) |
| Full suite command | `make && nxlink -a <switch_ip> SwitchPalace.nro` (deploy + manual test) |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| INST-01 | NSP installs to SD/NAND | manual | Deploy NRO, select NSP, install | No - Wave 0 |
| INST-02 | XCI installs to SD/NAND | manual | Deploy NRO, select XCI, install | No - Wave 0 |
| INST-03 | NSZ/XCZ decompress and install | manual | Deploy NRO, select NSZ, install | No - Wave 0 |
| INST-04 | Destination selection works | manual | Install to SD, then to NAND | No - Wave 0 |
| INST-05 | Hash verification shown | manual | Observe checkmark/X in progress UI | No - Wave 0 |
| INST-06 | Rollback on cancel/failure | manual | Cancel mid-install, verify no orphans | No - Wave 0 |
| TRUST-01 | Source fully open | audit | Review repo for binary blobs | No - Wave 0 |
| TRUST-02 | Reproducible build | unit | `docker build . && diff output.nro` | No - Wave 0 |
| TRUST-03 | No network calls on launch | audit | Review init code + network service calls | No - Wave 0 |
| TRUST-04 | Applet mode detection | manual | Launch as applet, verify badge + disabled items | No - Wave 0 |

### Sampling Rate
- **Per task commit:** `make` (successful compilation)
- **Per wave merge:** Build NRO + deploy to Switch for manual smoke test
- **Phase gate:** Full manual test of all INST-* and TRUST-* requirements on real hardware

### Wave 0 Gaps
- [ ] `Makefile` — devkitPro build system with borealis integration, C++17 flags
- [ ] `Dockerfile` — Reproducible build environment using devkitpro/devkita64
- [ ] `tests/host/` directory — Host-side unit tests for PFS0/HFS0 parsers (can run on PC without Switch)
- [ ] Test NSP/XCI/NSZ/XCZ fixture files (or hex dumps of headers for parser tests)

## Sources

### Primary (HIGH confidence)
- [libnx ncm.h](https://switchbrew.github.io/libnx/ncm_8h.html) — Full NCM content storage API: placeholder management, content registration, metadata database
- [libnx applet.h](https://switchbrew.github.io/libnx/applet_8h.html) — AppletType enum, appletGetAppletType() for mode detection
- [switchbrew.org XCI format](https://switchbrew.org/wiki/XCI) — XCI header structure, HFS0 partition layout, byte offsets
- [switchbrew.org NCA format](https://switchbrew.org/wiki/NCA) — NCA header structure, content types, encryption architecture
- [nicoboss/nsz DeepWiki](https://deepwiki.com/nicoboss/nsz/1.2-file-formats) — NCZ/NSZ/XCZ format specification: section headers, block compression, zstd stream layout
- [Awoo-Installer source](https://github.com/Huntereb/Awoo-Installer) — install.cpp, install_nsp.cpp, nca_writer.cpp: reference NCA installation pipeline
- [xfangfang/borealis](https://github.com/xfangfang/borealis) — UI framework: available views, XML layout, C++17 requirements, build integration

### Secondary (MEDIUM confidence)
- [switchbrew.org Setting up Development Environment](https://switchbrew.org/wiki/Setting_up_Development_Environment) — devkitPro setup instructions
- [devkitpro/devkita64 Docker](https://hub.docker.com/r/devkitpro/devkita64) — Official Docker image for Switch builds
- [switchbrew.org Homebrew ABI](https://switchbrew.org/wiki/Homebrew_ABI) — Heap override mechanism, applet type configuration
- [deepwiki libnx Getting Started](https://deepwiki.com/switchbrew/libnx/2-getting-started) — Heap management, __nx_heap_size override, build system

### Tertiary (LOW confidence)
- Community discussions on GBAtemp regarding applet mode memory limits (~40-80MB applet vs ~3.2GB application) — exact values unverified against official documentation
- [bandithedoge/devkitNix](https://github.com/bandithedoge/devkitNix) — Nix-based reproducible builds for devkitPro (alternative to Docker approach)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — devkitPro/libnx/borealis is the only viable path for Switch NRO development; well-documented
- Architecture: HIGH — NCA installation pipeline pattern verified across multiple open-source installers (Awoo, Goldleaf, Tinfoil docs)
- Pitfalls: HIGH — Common issues documented in issue trackers and community forums; NCZ re-encryption and placeholder cleanup are well-known challenges
- Applet memory limits: LOW — Exact numbers from community lore, not official docs
- borealis widget availability: MEDIUM — Confirmed 9 view types from source; progress bar absence needs validation against full source tree

**Research date:** 2026-03-25
**Valid until:** 2026-04-25 (stable ecosystem, slow-moving toolchain updates)
