# Architecture Research: SwitchPalace

**Domain:** Nintendo Switch homebrew content management
**Researched:** 2026-03-25
**Overall confidence:** MEDIUM-HIGH

## System Overview

SwitchPalace is a two-component system: a Switch NRO application (C/C++ on libnx) and a PC companion server (Go). The Switch app handles content installation, file management, and UI. The PC server handles high-throughput file serving over USB and network.

```
+------------------+          USB (DBI0-like protocol)          +------------------+
|                  | <========================================> |                  |
|   Switch NRO     |          Network (HTTP/FTP/SFTP)           |   PC Companion   |
|   (C/C++/libnx)  | <---------------------------------------- |   Server (Go)    |
|                  |          MTP (USB device mode)              |                  |
|                  | <========================================> |                  |
+------------------+                                            +------------------+
       |                                                               |
       v                                                               v
+------------------+                                            +------------------+
| Switch OS        |                                            | Local Filesystem |
| Services (IPC)   |                                            | (NSP/XCI/NCA     |
| - NCM (content)  |                                            |  game files)     |
| - ES (tickets)   |                                            +------------------+
| - NS (titles)    |
| - FS (files)     |
+------------------+
       |
       v
+------------------+
| Storage          |
| - SD Card        |
| - NAND (internal)|
+------------------+
```

## Switch NRO Architecture

### Runtime Environment

The NRO runs on Atmosphere CFW under the homebrew loader. libnx provides the core runtime: service IPC wrappers, threading primitives, USB device stack, and BSD socket support.

**Key libnx services used:**

| Service | Purpose | libnx Header |
|---------|---------|--------------|
| `ncm` | Content storage, placeholders, NCA registration | `switch/services/ncm.h` |
| `es` | Ticket import/deletion for encrypted content | `switch/services/es.h` |
| `ns` | Title management, application listing | `switch/services/ns.h` |
| `fs` | Filesystem access (SD, NAND, savedata) | `switch/services/fs.h` |
| `usbDs` | USB device mode (bulk endpoints) | `switch/services/usbds.h` |
| `bsd` | BSD sockets for network operations | `switch/services/bsd.h` |
| `account` | User account enumeration for save management | `switch/services/acc.h` |

**Confidence:** HIGH -- based on official libnx headers and switchbrew documentation.

### Threading Model

libnx provides POSIX-like threading with `Thread` objects backed by Switch kernel threads. Synchronization primitives include Mutex, CondVar, Barrier, Event, and Timer.

**Recommended thread architecture:**

```
Main Thread
  |-- UI rendering loop (borealis event loop, ~60fps vsync)
  |
  +-- Worker Thread Pool (2-4 threads)
  |     |-- Download/transfer workers
  |     |-- Installation pipeline workers
  |
  +-- USB Communication Thread
  |     |-- Dedicated bulk endpoint I/O
  |
  +-- Network I/O Thread(s)
        |-- HTTP client connections
        |-- FTP/SFTP client connections
        |-- FTP server (when in server mode)
```

The UI thread must never block. All I/O (USB, network, filesystem) runs on worker threads that post status updates back to the UI via a lock-free message queue or shared state protected by mutex.

**Confidence:** MEDIUM -- threading primitives are confirmed in libnx; the specific architecture is a design recommendation based on common patterns in Switch homebrew apps.

### UI Layer: Borealis

Use borealis (natinusala/borealis or xfangfang/borealis fork) for the UI. It provides:

- Hardware-accelerated rendering via nanovg (OpenGL ES on Switch)
- Flexbox layout engine (Yoga Layout)
- Controller navigation with automatic focus management
- Touch support
- XML-based view definitions
- Internationalization built in
- Cross-platform: compiles on PC for development/testing without a Switch

Borealis runs an event loop on the main thread. Custom views are created by subclassing `brls::View` or `brls::Box`. The app registers activities (screens) and pushes/pops them on a navigation stack.

**Confidence:** HIGH -- borealis is well-documented with active forks.

### NCA Installation Pipeline

This is the most critical subsystem. Installing content on Switch follows a precise sequence using NCM and ES services.

**Installation flow for an NSP (Nintendo Submission Package):**

```
1. PARSE NSP
   |-- Read PFS0 header (partition filesystem)
   |-- Extract file entries: NCAs, tickets (.tik), certs (.cert), CNMT
   |
2. IMPORT TICKETS
   |-- esImportTicket(ticket_data, cert_data)
   |-- Required BEFORE NCA registration for encrypted content
   |
3. FOR EACH NCA:
   |
   a. CREATE PLACEHOLDER
   |   ncmContentStorageGeneratePlaceHolderId(&placeholderId)
   |   ncmContentStorageCreatePlaceHolder(&storage, &contentId, &placeholderId, size)
   |
   b. STREAM DATA TO PLACEHOLDER
   |   Loop: ncmContentStorageWritePlaceHolder(&storage, &placeholderId, offset, data, data_size)
   |   (Write in chunks -- data can come from USB, network, or local file)
   |
   c. REGISTER NCA
   |   ncmContentStorageRegister(&storage, &contentId, &placeholderId)
   |   (Moves placeholder file to registered NCA path)
   |
4. REGISTER CONTENT META
   |-- ncmContentMetaDatabaseSet(&db, &key, cnmt_data, cnmt_size)
   |-- ncmContentMetaDatabaseCommit(&db)
   |
5. CLEANUP
   |-- ncmContentStorageClose(&storage)
   |-- ncmContentMetaDatabaseClose(&db)
```

**NCA storage paths on SD card:**
- Registered: `Nintendo/Contents/registered/<SHA256_byte_hex>/<nca_id>.nca/00` (split into 4GB chunks on FAT32)
- Placeholder: `Nintendo/Contents/placeholder/<placeholder_id>.nca`

**Critical detail:** The placeholder-based write system means installation can be streamed. You do not need the entire NCA in memory -- write chunks at offsets as they arrive. This is the key enabler for network-install-while-downloading.

**Confidence:** HIGH -- based on official switchbrew NCM documentation and libnx header analysis.

## PC Companion Architecture

### Why Go

Go is the recommended language for the PC companion server because:

1. **Goroutine-per-connection** model handles thousands of concurrent connections with minimal overhead (~2KB per goroutine)
2. **Built-in HTTP server** with native Range request support via `http.ServeContent()`
3. **Cross-platform** binary compilation (Windows, macOS, Linux) with zero dependencies
4. **USB support** via `gousb` (libusb bindings)
5. **Standard library** covers HTTP, FTP, filesystem watching -- minimal external dependencies
6. **Fast compilation** for rapid development iteration

Go 1.26 (current) includes 12-15% improvement in net/http high-concurrency request handling over Go 1.25.

For extreme throughput, `valyala/fasthttp` can replace `net/http` (handles 100K+ qps, 1.5M concurrent connections). However, standard `net/http` is sufficient for a PC-to-Switch file server where the bottleneck is USB or WiFi bandwidth, not HTTP overhead.

**Confidence:** HIGH for Go suitability; MEDIUM for fasthttp necessity (likely overkill).

### Server Component Design

```
PC Companion Server (Go)
|
+-- USB Module
|   |-- libusb device discovery (VID 0x057E, PID 0x3000)
|   |-- DBI0-compatible protocol handler
|   |-- Bulk endpoint I/O (IN/OUT)
|   |-- File range serving
|
+-- HTTP Module
|   |-- File server with Range request support
|   |-- Tinfoil-compatible JSON index endpoint
|   |-- Directory listing / file browsing API
|   |-- Parallel chunk coordination
|
+-- FTP Module (optional)
|   |-- FTP server for Switch FTP client access
|
+-- File Index
|   |-- Filesystem scanner for NSP/XCI/NCA files
|   |-- Metadata extraction (title ID, name, version)
|   |-- File watcher for live updates
|
+-- Configuration
    |-- Library paths
    |-- USB/network preferences
    |-- Tinfoil shop format export
```

### Serving Strategy for Maximum Throughput

**USB transfers:** The DBI0 protocol uses 1MB chunks (`BUFFER_SEGMENT_DATA_SIZE = 0x100000`). The bottleneck is USB 2.0 bandwidth (~35-40 MB/s practical) or USB 3.0 (~400 MB/s practical). The server should:
- Pre-read the next chunk while the current one transfers (double-buffering)
- Use memory-mapped file I/O for large files
- Minimize syscalls by reading in large aligned blocks

**Network transfers:** HTTP with Range requests enables the Switch to request multiple ranges in parallel from different goroutines. The server should:
- Support HTTP Range headers natively (Go's `http.ServeContent` does this automatically)
- Serve a Tinfoil-compatible JSON index at a known endpoint
- Support concurrent connections from the same Switch client

## Communication Protocols

### 1. USB Install Protocol (DBI0)

The existing DBI0 protocol is well-understood and should be supported for compatibility, then extended for performance.

**Packet structure:** 16 bytes, little-endian
```c
struct DBI0Header {
    char magic[4];      // "DBI0"
    uint32_t cmd_type;  // 0=REQUEST, 1=RESPONSE, 2=ACK
    uint32_t cmd_id;    // 0=EXIT, 1=LIST_DEPRECATED, 2=FILE_RANGE, 3=LIST
    uint32_t data_size; // payload size following header
};
```

**Protocol flow:**
```
PC (Server)                              Switch (Client)
    |                                         |
    |  <--- DBI0 Header (cmd=LIST) ---------- |  Switch requests file list
    |  --- DBI0 Header (RESPONSE, size) ----> |  Server sends header with list size
    |  <--- DBI0 Header (ACK) --------------- |  Switch acknowledges
    |  --- UTF-8 file list (newline-delim) -> |  Server sends filename list
    |                                         |
    |  <--- DBI0 Header (cmd=FILE_RANGE) ---- |  Switch requests file chunk
    |  --- DBI0 Header (ACK) --------------> |  Server acknowledges
    |  <--- range params (size,offset,name) - |  Switch sends range details
    |  --- DBI0 Header (RESPONSE, size) ----> |  Server confirms range
    |  <--- DBI0 Header (ACK) --------------- |  Switch acknowledges
    |  --- file data (1MB chunks) ----------> |  Server streams data
    |                                         |
    |  <--- DBI0 Header (cmd=EXIT) ---------- |  Switch signals completion
```

**Buffer constraints (Switch side):**
- USB buffers must be 0x1000-byte aligned (page-aligned)
- dcache must be flushed before PostBufferAsync
- Max transfer size per endpoint call: 0xFF0000 (~16MB)
- Practical chunk size: 1MB (0x100000) matching DBI convention
- Transfer hangs if size approaches 0x1000000 (~16MB) -- stay well below

**USB device identification:**
- Vendor ID: `0x057E` (Nintendo)
- Product ID: `0x3000` (Switch in homebrew USB mode)
- Windows requires libusbK or WinUSB driver (via Zadig)

**Confidence:** HIGH -- protocol extracted directly from dbibackend source code.

### 2. Network Install (HTTP)

The Switch NRO acts as an HTTP client that downloads content from the PC server (or community shop servers).

**Tinfoil-compatible shop index format:**
```json
{
    "files": [
        {
            "url": "https://server:port/path/to/game.nsp",
            "size": 4294967296
        }
    ],
    "directories": [
        "https://server:port/another/directory/"
    ],
    "success": "Connected to SwitchPalace server",
    "headers": ["Authorization: Basic ..."],
    "titledb": {
        "0100000000010000": {
            "name": "Game Title",
            "version": 0,
            "size": 4294967296
        }
    }
}
```

Supported URL schemes: `https://`, `http://`, `sdmc:/`, `gdrive:/`, `1f:/`

**Parallel chunked download architecture (the key speed improvement over DBI):**

```
Switch NRO Network Install Pipeline
|
+-- Chunk Scheduler
|   |-- Splits target file into N chunks (e.g., 8-16MB each)
|   |-- Assigns chunks to download workers
|   |-- Tracks completion status per chunk
|
+-- Download Worker Pool (4-8 workers)
|   |-- Each worker: HTTP GET with Range header
|   |-- Workers write to a shared chunk buffer ring
|   |-- Signal completion to scheduler
|
+-- Installation Writer (single thread)
|   |-- Consumes completed chunks IN ORDER
|   |-- Calls ncmContentStorageWritePlaceHolder at correct offsets
|   |-- Handles out-of-order chunk arrival via reorder buffer
|
+-- Progress Reporter
    |-- Aggregates throughput from all workers
    |-- Updates UI via message queue
```

This producer-consumer pattern allows download and installation to overlap. The Switch writes to the NCA placeholder at sequential offsets while downloading chunks in parallel. The reorder buffer handles the fact that chunk 5 might complete before chunk 3.

**Confidence:** MEDIUM -- the parallel chunked download is a design, not an existing proven Switch implementation. The NCM placeholder write-at-offset API confirms this is technically feasible.

### 3. MTP Responder (USB)

MTP (Media Transfer Protocol) allows the Switch to appear as a media device when connected to a PC. The PC uses standard MTP tools (Windows Explorer, Android File Transfer) to browse and transfer files.

**Architecture:**
```
Switch MTP Responder
|
+-- USB Device Layer (usbDs)
|   |-- Bulk IN/OUT endpoints for MTP data
|   |-- Interrupt endpoint for MTP events
|
+-- MTP Protocol Layer
|   |-- MTP operation parser (OpenSession, GetDeviceInfo, etc.)
|   |-- MTP response builder
|   |-- Object handle management
|
+-- Storage Abstraction
|   |-- SD card storage backend
|   |-- NAND storage backend (read-only recommended)
|   |-- Virtual "Install" folder (drag-and-drop triggers install)
|
+-- Install Hook
    |-- Detects files written to Install virtual folder
    |-- Triggers NCA installation pipeline
```

Reference implementation: `retronx-team/mtp-server-nx` (Apache 2.0, C++). Known limitation: slow startup when scanning many files on SD card. SwitchPalace should use lazy enumeration or cached file indices.

**Confidence:** MEDIUM -- MTP responder architecture based on mtp-server-nx reference; virtual install folder is a design proposal.

### 4. FTP Server Mode

The Switch runs an FTP server allowing wireless file management from any FTP client.

**Architecture:** Standard single-threaded FTP server using libnx BSD sockets. Control connection on port 5000 (configurable), PASV mode for data connections. Maps FTP paths to SD card and NAND filesystems.

**Confidence:** HIGH -- FTP server is a well-understood pattern; multiple Switch homebrew FTP servers exist as reference.

## Key Architecture Patterns

### Pattern 1: Streaming Installation Pipeline

**What:** Never buffer an entire NCA in memory. Stream data from source (USB/network) directly into NCM placeholder writes.

**Why:** Switch has limited RAM (~3.2GB total, homebrew gets a fraction). Game NCAs can be 10-30GB.

```
Source -> Chunk Buffer (1-4MB) -> ncmContentStorageWritePlaceHolder(offset) -> repeat
```

### Pattern 2: Service Session Management

**What:** Open libnx service sessions (NCM, ES, NS) on demand, not at startup. Close them promptly after use.

**Why:** The Switch OS limits concurrent IPC sessions. Holding sessions open blocks other homebrew or system processes. Open `ncmContentStorage` when starting an install, close it when done.

### Pattern 3: Double-Buffered USB Transfers

**What:** Allocate two page-aligned USB buffers. While one is being sent/received via `PostBufferAsync`, fill/process the other.

**Why:** USB bulk transfers are asynchronous (`PostBufferAsync` returns immediately with a UrbId). Overlapping I/O with processing maximizes throughput.

```c
// Pseudocode
while (data_remaining) {
    PostBufferAsync(endpoint, buffer_a, chunk_size);  // Start async transfer
    process(buffer_b);                                  // Process previous chunk
    WaitForCompletion(endpoint);                        // Wait for transfer A
    swap(buffer_a, buffer_b);                           // Swap buffers
}
```

### Pattern 4: Decoupled UI Updates

**What:** UI thread polls a shared state struct at render time rather than being pushed updates.

**Why:** Avoids lock contention. Worker threads atomically update progress counters; UI reads them each frame.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Blocking the UI Thread on I/O

**What:** Making USB, network, or filesystem calls on the main borealis event loop thread.
**Why bad:** Freezes the UI, makes the app appear hung. Users will think it crashed.
**Instead:** All I/O on worker threads. UI thread only reads shared state and renders.

### Anti-Pattern 2: Allocating Large Buffers on Stack

**What:** Declaring multi-MB arrays as local variables in functions.
**Why bad:** Switch homebrew has limited stack size. Will cause crashes.
**Instead:** Heap-allocate large buffers with `malloc` or `aligned_alloc` (0x1000 alignment for USB buffers).

### Anti-Pattern 3: Holding NCM Sessions Open Across Operations

**What:** Opening `ncmContentStorage` at app startup and never closing it.
**Why bad:** Blocks system content management. May cause issues with other homebrew or system updates.
**Instead:** Open session, perform operation(s), close session. Group related operations to avoid excessive open/close overhead.

### Anti-Pattern 4: Sequential NCA Installation

**What:** Downloading one NCA completely, then installing it, then downloading the next.
**Why bad:** Wastes time. Download and install can overlap because of the placeholder write-at-offset API.
**Instead:** Pipeline: download chunks and write to placeholder simultaneously.

## Component Dependency Graph

```
Layer 0 (no dependencies -- build first):
  - libnx service wrappers (NCM, ES, NS, FS, USB, BSD)
  - Buffer management utilities (aligned alloc, ring buffers)
  - Configuration/settings storage

Layer 1 (depends on Layer 0):
  - NCA installation pipeline (NCM placeholder workflow)
  - USB bulk transfer layer (usbDs endpoint management)
  - Network client (HTTP with Range support, BSD sockets)
  - Filesystem browser (FS service wrapper)

Layer 2 (depends on Layer 1):
  - USB install protocol handler (DBI0 command loop + installation pipeline)
  - Network install manager (parallel downloader + installation pipeline)
  - MTP responder (USB layer + filesystem + install hook)
  - FTP server (network layer + filesystem)
  - Title manager (NCM + NS queries)
  - Ticket manager (ES service wrapper)

Layer 3 (depends on Layer 2):
  - Borealis UI views for each feature
  - Activity log / play time tracker
  - Save backup/restore manager

PC Companion (independent, parallel development):
  Layer 0: USB device discovery, file indexer
  Layer 1: DBI0 protocol server, HTTP file server
  Layer 2: Tinfoil index generator, configuration UI (CLI or TUI)
```

## Build Order Implications

### Phase 1: Foundation (Switch)
Build the NCA installation pipeline first. It is the critical path -- every feature depends on it. Validate with local SD card installation (read NSP from SD, install via NCM).

**Why first:** If NCA installation does not work correctly, nothing else matters. It touches the most sensitive system services (NCM, ES) and has the most potential for bricking-adjacent bugs.

### Phase 2: USB Communication
Build the DBI0 protocol handler on both Switch and PC sides. This gives a concrete end-to-end data path for testing the installation pipeline with PC-sourced files.

**Why second:** USB is simpler than network (no DNS, no TLS, deterministic connection). Provides the fastest test loop for the installation pipeline.

### Phase 3: Network Install
Add HTTP client with parallel chunk downloading. This is the primary differentiator over DBI (speed improvement). Requires the installation pipeline from Phase 1 and careful buffer management.

**Why third:** Network adds complexity (timeouts, retries, TLS, parallel coordination). Build on a proven installation pipeline.

### Phase 4: MTP + File Management
Add MTP responder and file browser. These are convenience features that rely on the USB and filesystem layers already built.

### Phase 5: Game Management + Polish
Title listing, save backup, ticket management, activity log. These are independent features that use already-built service wrappers.

### PC Companion Phases (can run in parallel with Switch development):
1. USB server (DBI0 protocol) -- needed for Switch Phase 2 testing
2. HTTP file server with Tinfoil index -- needed for Switch Phase 3 testing
3. File indexer and metadata extraction
4. Configuration and CLI/TUI interface

**The PC USB server should be built slightly ahead of the Switch USB client** so the Switch side has something to test against.

## Scalability Considerations

| Concern | Typical Use | Power User | Notes |
|---------|------------|------------|-------|
| Concurrent installs | 1 at a time | Queue of 10-20 | Queue system, not parallel installs (NCM contention) |
| Game library size | 50-200 titles | 500-2000+ | Lazy loading in UI, paginated NCM queries |
| File sizes | 1-16 GB per game | Up to 32 GB | Streaming install handles any size |
| Network speed | 20-50 Mbps WiFi | 100 Mbps ethernet via dock | Parallel chunks help most on high-bandwidth connections |
| USB throughput | 35 MB/s (USB 2.0) | 400 MB/s (USB 3.0 via dock) | Double-buffering critical for USB 3.0 saturation |

## Sources

- [libnx GitHub Repository](https://github.com/switchbrew/libnx) -- HIGH confidence
- [NCM Services - SwitchBrew Wiki](https://switchbrew.org/wiki/NCM_services) -- HIGH confidence
- [libnx NCM Header](https://switchbrew.github.io/libnx/ncm_8h.html) -- HIGH confidence
- [USB Services - SwitchBrew Wiki](https://switchbrew.org/wiki/USB_services) -- HIGH confidence
- [SD Filesystem - SwitchBrew Wiki](https://switchbrew.org/wiki/SD_Filesystem) -- HIGH confidence
- [dbibackend Source Code](https://github.com/lunixoid/dbibackend/blob/master/dbibackend/dbibackend.py) -- HIGH confidence (protocol details)
- [Tinfoil Custom Index Documentation](https://blawar.github.io/tinfoil/custom_index/) -- HIGH confidence
- [borealis UI Library](https://github.com/natinusala/borealis) -- HIGH confidence
- [mtp-server-nx](https://github.com/retronx-team/mtp-server-nx) -- MEDIUM confidence (reference architecture)
- [Goldleaf](https://github.com/XorTroll/Goldleaf) -- MEDIUM confidence (installation flow reference)
- [Go net/http Performance](https://eli.thegreenplace.net/2019/on-concurrency-in-go-http-servers/) -- MEDIUM confidence
- [fasthttp](https://github.com/valyala/fasthttp) -- MEDIUM confidence
- [NCA Format - SwitchBrew Wiki](https://switchbrew.org/wiki/NCA) -- HIGH confidence
