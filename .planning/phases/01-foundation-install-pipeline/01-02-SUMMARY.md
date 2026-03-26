---
phase: 01-foundation-install-pipeline
plan: 02
subsystem: install
tags: [ncm, nca, ncz, zstd, aes-ctr, sha256, pfs0, hfs0, libnx, mbedtls]

# Dependency graph
requires:
  - phase: 01-foundation-install-pipeline/01
    provides: "Project scaffold, PFS0/HFS0 parsers, Install base class, type contracts"
provides:
  - "NCM service wrapper with placeholder lifecycle and rollback (ContentManager)"
  - "NCA streaming writer with standard and NCZ (zstd decompression + AES-CTR re-encryption) support"
  - "SHA-256 streaming hash and AES-CTR encryption utilities"
  - "NSP installer (PFS0 pipeline with ticket import)"
  - "XCI installer (HFS0 secure partition pipeline)"
  - "Already-installed detection via NCM content queries"
  - "Stale placeholder cleanup for crash recovery"
  - "Format detection for NSP/XCI/NSZ/XCZ"
affects: [01-foundation-install-pipeline/03, ui, testing]

# Tech tracking
tech-stack:
  added: [switch-zstd, mbedtls, libnx-crypto]
  patterns: [streaming-nca-pipeline, placeholder-rollback, ncz-decompress-reencrypt]

key-files:
  created:
    - source/nx/content_mgmt.cpp
    - source/nx/ticket.cpp
    - source/install/nca_writer.cpp
    - source/util/crypto.cpp
    - source/install/nsp.cpp
    - source/install/xci.cpp
  modified:
    - include/nx/content_mgmt.hpp
    - include/install/nca_writer.hpp
    - include/util/crypto.hpp
    - include/install/nsp.hpp
    - include/install/xci.hpp

key-decisions:
  - "SHA256 uses pimpl pattern to avoid mbedtls in public header"
  - "AES-CTR uses libnx hardware-accelerated Aes128CtrContext on Switch, memcpy stub on host"
  - "NCZ counter adjustment uses big-endian increment on last 8 bytes of AES-CTR counter"
  - "detectFormat placed in nsp.cpp as free function declared in install.hpp"

patterns-established:
  - "Streaming NCA pipeline: ReadFunc lambda + chunk-based write to NCM placeholder"
  - "Placeholder tracking: ContentManager::trackPlaceholder auto-called on create, rollbackAll deletes all"
  - "NCZ decompression: 0x4000 header passthrough, NCZSECTN parse, zstd stream, per-section AES-CTR re-encrypt"
  - "Host compilation: #ifdef __SWITCH__ guards with stubs for all libnx/mbedtls/zstd calls"

requirements-completed: [INST-01, INST-02, INST-03, INST-05, INST-06]

# Metrics
duration: 5min
completed: 2026-03-26
---

# Phase 01 Plan 02: Install Pipeline Engine Summary

**NCA streaming pipeline engine: NCM placeholder management with rollback, NSP/XCI installers, NCZ zstd decompression with per-section AES-CTR re-encryption, SHA-256 hash verification, and ticket import via ES service**

## Performance

- **Duration:** 5 min
- **Started:** 2026-03-26T00:18:47Z
- **Completed:** 2026-03-26T00:23:41Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments
- Full NCM service wrapper with placeholder create/write/register/delete, meta database, application record, rollback tracking, and stale cleanup
- NCA streaming writer supporting both standard NCAs (direct chunk write) and NCZ (zstd decompress + per-section AES-CTR re-encrypt)
- NSP installer: PFS0 parse, CNMT-first processing, NCA streaming, NCZ detection for NSZ, ticket/cert import, hash verification, cancel + rollback
- XCI installer: root HFS0 at 0xF000, secure partition discovery, same NCA pipeline, cancel + rollback
- Already-installed detection via NCM hasContent query before install begins
- Format detection mapping all four extensions (NSP/XCI/NSZ/XCZ)

## Task Commits

Each task was committed atomically:

1. **Task 1: NCM wrapper, NCA writer with NCZ support, crypto, and ticket import** - `3a22e80` (feat)
2. **Task 2: NSP and XCI installer implementations with already-installed detection** - `777a7a7` (feat)

## Files Created/Modified
- `source/nx/content_mgmt.cpp` - Full NCM service wrapper: storage, placeholders, meta database, app records, rollback
- `source/nx/ticket.cpp` - ES service ticket + certificate import
- `source/install/nca_writer.cpp` - Standard NCA streaming + NCZ zstd decompression with AES-CTR re-encryption
- `source/util/crypto.cpp` - SHA-256 via mbedtls, AES-CTR via libnx hardware-accelerated crypto
- `source/install/nsp.cpp` - NSP installer: PFS0 pipeline, CNMT-first, tickets, hash verify, detectFormat
- `source/install/xci.cpp` - XCI installer: HFS0 secure partition pipeline, hash verify
- `include/nx/content_mgmt.hpp` - Updated with concrete libnx types and full method signatures
- `include/install/nca_writer.hpp` - Updated with ReadFunc, writeStandardNca, writeNczNca, parseNczSections
- `include/util/crypto.hpp` - Updated with pimpl SHA256 class, non-copyable
- `include/install/nsp.hpp` - Updated with ContentManager member, header data storage
- `include/install/xci.hpp` - Updated with secure partition offset, ContentManager member

## Decisions Made
- SHA256 class uses pimpl pattern (forward-declared Impl struct) to avoid exposing mbedtls types in the public header -- keeps host compilation clean
- AES-CTR encryption uses libnx Aes128CtrContext (hardware-accelerated via SPL service) on Switch, with a simple memcpy stub for host builds
- NCZ counter adjustment uses big-endian increment on the last 8 bytes of the AES-CTR counter, matching the NSZ format specification
- detectFormat free function placed in nsp.cpp alongside the NSP implementation since it was declared in install.hpp

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Install pipeline engine is complete and ready for UI integration (Plan 03)
- NSP and XCI code paths both share NcaWriter and ContentManager, so NSZ/XCZ are handled automatically via NCZ detection
- All four format types (NSP, XCI, NSZ, XCZ) can be processed through the unified pipeline
- Rollback safety ensures no orphaned placeholders on failure or cancellation

---
*Phase: 01-foundation-install-pipeline*
*Completed: 2026-03-26*
