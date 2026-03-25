# Stack Research: SwitchPalace

**Project:** SwitchPalace - Open-source Nintendo Switch content manager
**Researched:** 2026-03-25
**Overall Confidence:** MEDIUM-HIGH

---

## Recommended Stack

### Switch NRO (Console Application)

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| devkitA64 | r23+ (latest via pacman) | Cross-compiler toolchain (AArch64) | Only supported toolchain for Switch homebrew. No alternative exists. | HIGH |
| libnx | v4.11.1 | Switch homebrew SDK | The standard (and only real) userland library for Switch homebrew. Wraps all HOS services (ncm, ns, nim, nifm, usb, fs). Released Feb 2025 with firmware 21.0.0 support. | HIGH |
| borealis (xfangfang fork) | v2.1.0+ | UI framework | Hardware-accelerated, Nintendo Switch-native look and feel. XML-based layout with flexbox engine. The xfangfang fork is the most actively maintained (1,227+ commits, drives the wiliwili project). Use this over natinusala/borealis (original, less maintained) or XITRIX fork. | MEDIUM-HIGH |
| switch-curl | latest via dkp-pacman | HTTP/HTTPS client | devkitPro-packaged libcurl with libnx TLS backend. Required for network installs from HTTP/HTTPS servers. Supports system proxy via nifm. | HIGH |
| switch-mbedtls | latest via dkp-pacman | TLS/SSL | Provides HTTPS support for switch-curl. Now uses libnx native TLS backend (not standalone mbedtls). | HIGH |
| switch-zlib | latest via dkp-pacman | Compression | Required for NSZ/XCZ decompression during content installation. | HIGH |
| libnx usbComms / usbDs | built-in to libnx | USB device-mode communication | libnx provides both high-level usbComms (simple read/write) and low-level usbDs (USB device stack) APIs. Use usbDs for custom high-throughput protocol. usbDsGetSpeed added in v4.11.0 for USB 3.0 detection. | HIGH |
| libnx ncm/ns services | built-in to libnx | Content installation | NCM (Content Meta) and NS (Nintendo Shell) services handle title installation, content management, ticket operations. This is how NSP/XCI content gets installed to NAND/SD. | HIGH |
| libnx fs service | built-in to libnx | Filesystem access | SD card browsing, NAND partition access, save data management. | HIGH |

#### Key libnx Services for SwitchPalace

| Service | Header | Purpose |
|---------|--------|---------|
| `ncm` | `switch/services/ncm.h` | Content meta database, placeholder management, content storage |
| `ns` | `switch/services/ns.h` | Application management, installation, control data (icons, titles) |
| `nim` | `switch/services/nim.h` | Network install manager (if using system-level install flow) |
| `nifm` | `switch/services/nifm.h` | Network interface management, connectivity checks |
| `es` | `switch/services/es.h` | eTicket/rights management |
| `fs` | `switch/services/fs.h` | Filesystem operations, save data, SD/NAND access |
| `usb` | `switch/runtime/devices/usb_comms.h` | USB device communication |
| `account` | `switch/services/acc.h` | User account enumeration (for save management) |
| `pdm` | `switch/services/pdm.h` | Play data management (activity log, play time stats) |

### PC Companion Server

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| **Go** | 1.23+ | Server language | Go over Rust for the companion server. Rationale below. | MEDIUM-HIGH |
| google/gousb | latest | USB communication | Google-maintained libusb wrapper for Go. Supports bulk transfers required for Switch USB protocol. Battle-tested in production USB tooling. | MEDIUM |
| net/http (stdlib) | built-in | HTTP server | Serves content to Switch over network install. Go's stdlib HTTP server handles concurrent connections well out of the box. | HIGH |
| gorilla/mux or chi | latest | HTTP routing | Lightweight routing for the REST API that the Switch NRO will call for browsing/selecting content. chi is preferred (maintained, stdlib-compatible). | MEDIUM |
| pkg/sftp + crypto/ssh | latest | SFTP client | For browsing SFTP sources. Go has excellent SSH/SFTP libraries. | MEDIUM |
| jlaffaye/ftp | latest | FTP client | For browsing FTP sources. Widely used Go FTP client. | MEDIUM |

#### Why Go Over Rust

The PC companion is primarily I/O-bound (USB bulk transfers, HTTP serving, file reading). In I/O-bound workloads, Go and Rust perform comparably -- the bottleneck is USB/network throughput, not CPU. Go wins on:

1. **Development velocity:** Faster to write, easier to maintain for a community open-source project where contributors matter.
2. **Cross-compilation:** `GOOS=windows GOARCH=amd64 go build` produces a single static binary. No complex toolchain setup. Rust cross-compilation works but requires more setup for Windows targets with USB dependencies.
3. **USB library maturity:** google/gousb is stable and production-proven. Rust's nusb is newer (pure Rust, no libusb dependency) but less battle-tested for high-throughput bulk transfers.
4. **Contributor accessibility:** More developers know Go than Rust. For an open-source project aiming for community contributions, this matters significantly.
5. **Sufficient performance:** DBI's backend is Python (pyusb). Go is 10-50x faster than Python for I/O operations. We do not need Rust-level performance to massively outperform DBI.

**When Rust would be better:** If the companion needed heavy CPU work (compression, encryption at scale) or if this were a solo developer project where contributor accessibility does not matter. Neither applies here.

### Build System and CI

| Technology | Purpose | Why | Confidence |
|------------|---------|-----|------------|
| devkitPro Makefile system | NRO build | Standard build system for all Switch homebrew. Uses devkitA64 `switch_rules` includes. Every libnx project uses this. | HIGH |
| CMake (for borealis) | UI library build | borealis uses CMake. The NRO Makefile will invoke CMake for the borealis submodule. | HIGH |
| Go modules + `go build` | PC server build | Standard Go build system. No external build tool needed. | HIGH |
| `devkitpro/devkita64` Docker image | CI builds for NRO | Official devkitPro Docker image with full toolchain + portlibs pre-installed. Use in GitHub Actions. | HIGH |
| GitHub Actions | CI/CD | Standard for open-source projects. Free for public repos. Workflow: Docker-based NRO build + native Go build + release artifacts. | HIGH |
| Makefile (top-level) | Orchestration | Top-level Makefile that builds both NRO and PC server, creates release archives. | HIGH |

#### CI Pipeline Structure

```yaml
# Conceptual GitHub Actions workflow
jobs:
  build-nro:
    runs-on: ubuntu-latest
    container: devkitpro/devkita64:latest
    steps:
      - checkout
      - make -C switch-app
      - upload-artifact: SwitchPalace.nro

  build-pc-server:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        goos: [windows, linux, darwin]
    steps:
      - checkout
      - go build -o switchpalace-server
      - upload-artifact per OS

  release:
    needs: [build-nro, build-pc-server]
    # Create GitHub release with all artifacts
```

### Testing Approach

| Layer | Approach | Why | Confidence |
|-------|----------|-----|------------|
| NRO unit logic | Compile-time tests + PC-side test harness | Switch homebrew has no test framework. Extract pure logic (protocol parsing, data structures) into testable C++ modules that compile on PC with standard test frameworks (Catch2 or doctest). | MEDIUM |
| USB protocol | Mock USB endpoint on PC | Test the USB protocol state machine by mocking the USB read/write layer. Both sides (Go server + C++ NRO logic) should have protocol tests. | MEDIUM |
| Network protocol | Integration tests with mock HTTP server | Test Tinfoil-format shop parsing, chunked download logic against a local HTTP server serving test fixtures. | MEDIUM-HIGH |
| PC server | Standard Go testing (`go test`) | Go has excellent built-in test tooling. Test USB protocol, HTTP server, file serving. | HIGH |
| End-to-end | Manual testing on real hardware | No emulator accurately reproduces Switch HOS services. Real hardware testing is unavoidable for content installation, USB communication, and UI testing. | HIGH |
| UI | Visual verification on Switch | borealis renders differently on Switch vs PC. PC builds help iterate fast, but Switch testing is required for final verification. | MEDIUM |

---

## Protocol Compatibility Requirements

### Tinfoil Shop Protocol (Network Install)

SwitchPalace must implement a client compatible with the Tinfoil/DBI shop format:

| Aspect | Specification |
|--------|---------------|
| Transport | HTTP/HTTPS (primary), FTP/FTPS, SMB |
| Index format | JSON with file URLs, or HTML directory listings with parseable links |
| File naming | Must contain `[titleid]` in filename for indexing |
| Authentication | Optional HTTP Basic Auth (username/password in shop config) |
| Special URLs | `gdrive:`, `1f:`, `dropbox://`, `jbod:` schemes (lower priority) |

### USB Install Protocol

The USB protocol between Switch and PC follows a custom bulk-transfer protocol (not standard MTP for installs):

| Aspect | Detail |
|--------|--------|
| Driver | libusbK or WinUSB on Windows (must document for users) |
| Transfer mode | USB bulk transfers via libnx usbDs / usbComms |
| USB 3.0 | Supported when `usb30_force_enabled = u8!0x1` in Atmosphere system_settings.ini |
| Protocol | Custom command/response protocol over bulk endpoints (study Awoo-Installer and tinfoilusbgo for reference implementation) |

### MTP Protocol

DBI's MTP responder mode exposes virtual drives. SwitchPalace should implement MTP responder on the Switch side for drag-and-drop file management (separate from the custom USB install protocol).

---

## Installation Commands

### Switch Development Environment

```bash
# Install devkitPro pacman (platform-specific installer from devkitpro.org)
# Then install Switch development packages:
sudo dkp-pacman -S switch-dev       # devkitA64 + libnx + switch-tools
sudo dkp-pacman -S switch-curl      # libcurl for HTTP/HTTPS
sudo dkp-pacman -S switch-mbedtls   # TLS support
sudo dkp-pacman -S switch-zlib      # zlib compression
sudo dkp-pacman -S switch-freetype  # Font rendering (if not using borealis built-in)
sudo dkp-pacman -S switch-libpng    # PNG support for icons/images
```

### PC Companion (Go)

```bash
# Initialize Go module
go mod init github.com/switchpalace/switchpalace-server

# Core dependencies
go get github.com/google/gousb       # USB communication
go get github.com/go-chi/chi/v5      # HTTP routing
go get github.com/pkg/sftp           # SFTP client
go get github.com/jlaffaye/ftp       # FTP client
go get golang.org/x/crypto/ssh       # SSH for SFTP
```

### borealis (Git Submodule)

```bash
# Add as submodule in the NRO project
git submodule add https://github.com/xfangfang/borealis.git libs/borealis
```

---

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| UI Framework | borealis (xfangfang) | LVGL | LVGL is lower-level and requires more manual work for Switch-native look. borealis gives Nintendo-style UI out of the box with XML layouts. |
| UI Framework | borealis (xfangfang) | SDL2 + custom UI | Enormous effort to build a full UI toolkit. borealis already solves this. |
| UI Framework | borealis (xfangfang) | borealis (natinusala original) | Original is less actively maintained. xfangfang fork has more recent commits and proven in production (wiliwili). |
| PC Language | Go | Rust | Overkill for I/O-bound workload. Higher contributor barrier. See detailed rationale above. |
| PC Language | Go | Python | DBI uses Python (pyusb) and it is the primary reason for slow transfer speeds. Python's GIL and overhead make it unsuitable for high-throughput USB/network I/O. |
| PC Language | Go | C++ | Unnecessary complexity for a companion tool. No memory safety, harder to cross-compile, slower development. |
| USB (Go) | google/gousb | karalabe/usb | karalabe/usb is HID-focused, not optimized for bulk transfer workflows needed here. |
| USB (Rust, if chosen) | nusb | rusb | nusb is pure Rust (no libusb C dependency), making distribution easier. But we recommend Go, making this moot. |
| Build System | Makefile + CMake | Meson | devkitPro ecosystem is Makefile-based. Fighting the ecosystem adds friction for no benefit. |
| CI | GitHub Actions | GitLab CI | Project will be on GitHub (open source community). GitHub Actions is the natural fit. |

---

## What NOT to Use

### Do NOT use nx.js (JavaScript runtime for Switch)
While nx.js exists as a JavaScript runtime for Switch homebrew, it adds a massive runtime overhead (V8/QuickJS engine) and cannot access low-level HOS services (ncm, ns, es) needed for content installation. It is designed for simple apps, not system-level tools.

### Do NOT use Python for the PC companion
DBI's dbibackend uses Python with pyusb. This is the primary bottleneck for DBI's slow USB transfers. Python's GIL, interpreted nature, and pyusb overhead make it fundamentally unsuitable for high-throughput data transfer.

### Do NOT use ImGui for Switch UI
While ImGui works on Switch, it produces a PC-application aesthetic that feels foreign on a console. borealis provides the native Nintendo Switch UI paradigm (sidebar navigation, list views, controller-first interaction) that users expect.

### Do NOT implement your own TLS stack
Use switch-curl with the libnx TLS backend. Rolling your own TLS on an embedded system is a security and maintenance nightmare.

### Do NOT use the original natinusala/borealis
The original borealis repository has not seen the same level of maintenance as the xfangfang fork. Multiple homebrew projects have migrated to community forks for this reason.

### Do NOT bypass devkitPro's package manager
Installing libnx or portlibs manually (from source) instead of via `dkp-pacman` leads to version mismatches and build failures. Always use the package manager.

---

## Confidence Assessment

| Area | Confidence | Reasoning |
|------|------------|-----------|
| devkitA64 + libnx | HIGH | Only toolchain for Switch homebrew. v4.11.1 verified on GitHub releases (Feb 2025). No alternatives exist. |
| borealis (xfangfang) | MEDIUM-HIGH | Verified active on GitHub (v2.1.0, July 2024). Recommended by community. Small risk: fork maintenance could slow. Mitigation: borealis API is stable, could switch forks. |
| libnx services (ncm/ns/fs/usb) | HIGH | Core libnx functionality, documented in header files, used by every content installer (Awoo, Goldleaf, Tinfoil). |
| Go for PC companion | MEDIUM-HIGH | Sound reasoning (I/O-bound, contributor accessibility, sufficient performance). Risk: gousb may need performance tuning for maximum USB 3.0 throughput. Mitigation: profile early. |
| google/gousb | MEDIUM | Google-maintained, stable, supports bulk transfers. Risk: libusb C dependency complicates some builds. Mitigation: static linking or vendored libusb. |
| switch-curl / portlibs | HIGH | Official devkitPro packages, installed via pacman. Standard approach for all networked Switch homebrew. |
| CI with Docker | HIGH | devkitpro/devkita64 Docker image is official and widely used. Standard pattern for Switch homebrew CI. |
| Tinfoil protocol compat | MEDIUM | Protocol is not formally specified; reverse-engineered from Tinfoil/NUT/DBI behavior. Will need testing against real shop servers. |
| USB install protocol | MEDIUM | Custom protocol, not formally documented. Reference implementations exist (Awoo-Installer, tinfoilusbgo, NUT). Will require careful protocol analysis. |
| Testing approach | MEDIUM | No standard test framework for Switch homebrew. The "extract pure logic and test on PC" approach is sound but requires discipline. |

---

## Key Sources

- [libnx releases (GitHub)](https://github.com/switchbrew/libnx/releases) - v4.11.1, Feb 2025
- [libnx API documentation](https://switchbrew.github.io/libnx/)
- [borealis - xfangfang fork (GitHub)](https://github.com/xfangfang/borealis) - v2.1.0, July 2024
- [devkitPro Getting Started](https://devkitpro.org/wiki/Getting_Started)
- [Setting up Development Environment (switchbrew)](https://switchbrew.org/wiki/Setting_up_Development_Environment)
- [google/gousb (GitHub)](https://github.com/google/gousb)
- [Tinfoil Network Install docs](https://blawar.github.io/tinfoil/network/)
- [DBI (GitHub)](https://github.com/rashevskyv/dbi)
- [Awoo-Installer (GitHub)](https://github.com/Huntereb/Awoo-Installer) - reference for NSP install flow
- [tinfoilusbgo (GitHub)](https://github.com/bycEEE/tinfoilusbgo) - reference for Go USB protocol
- [devkitPro Docker images](https://github.com/devkitPro)
- [Rust vs Go performance benchmarks 2025](https://blog.jetbrains.com/rust/2025/06/12/rust-vs-go/)
- [nusb - pure Rust USB library](https://github.com/kevinmehall/nusb)
