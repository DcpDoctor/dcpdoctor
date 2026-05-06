# DcpDoctor

A comprehensive, professional-grade DCP (Digital Cinema Package) validator, analyzer, and diagnostic tool. Built in C++23 for maximum performance.

DcpDoctor validates DCPs against SMPTE ST 429/ST 2067, Interop, and BV2.1 standards with the depth and precision required for theatrical distribution.

## Features

### Core Validation
- **Structure validation** — ASSETMAP, PKL, CPL parsing with full cross-referencing
- **Hash verification** — SHA-1 integrity checking for all assets (with SQLite cache for speed)
- **XML digital signatures** — X.509 certificate chain and signature verification
- **Schema validation** — Full XML validation against SMPTE and Interop schemas
- **Duplicate detection** — Identifies duplicate asset IDs across packages

### Standards Compliance
- **SMPTE ST 429** — Complete SMPTE DCP standard validation
- **Interop** — Legacy Interop DCP support
- **BV2.1 (Bv2.1)** — SMPTE Best Practices for theatrical distribution:
  - ASSETMAP.xml naming enforcement
  - PKL .xml extension check
  - ContentVersion element requirement
  - ExtensionMetadata presence
  - MainMarkers in first reel
  - Approved EditRate validation (24/25/30/48/60 fps)
- **ISDCF Naming** — Content title naming convention validation

### Picture Validation
- **J2K bitrate analysis** — Per-frame bitrate statistics (min/max/avg)
- **DCI bitrate limits** — 250 Mbps (2K) / 500 Mbps (4K) enforcement
- **Deep J2K codestream** — Profile (RSIZ), decomposition levels, code-block sizes, wavelet type, component validation
- **4K/2K detection** — Resolution and aspect ratio verification

### Sound Validation
- **Audio level analysis** — Per-channel peak and RMS in dBFS
- **Clipping detection** — Flags audio near 0 dBFS
- **Silence detection** — Warns on channels below -80 dBFS
- **Channel count** — Validates channel configuration
- **MCA labeling** — Multi-Channel Audio label presence check
- **Audio sync drift** — Detects picture/sound duration mismatches per reel

### Subtitle & Caption Validation
- **SMPTE ST 429-5** timed text support
- **Timing validation** — TimeIn/TimeOut ordering and overlap detection
- **Required element checks** — ReelNumber, Language, LoadFont
- **SubtitleID presence** — Unique identifier validation

### Encryption & Security
- **Encrypted content detection** — Identifies encrypted MXF assets
- **KDM validation** — Parse and validate Key Delivery Messages:
  - Validity period checking (expired / not-yet-valid)
  - CPL reference cross-validation against DCP
  - Content title extraction

### Dolby Atmos
- **IAB detection** — Identifies Immersive Audio Bitstream containers
- **DC Data track** — Detects Dolby Atmos auxiliary data tracks

### Reel & Structure Analysis
- **Reel continuity** — Validates sequential entry points across reels
- **Stereo 3D** — Checks left/right eye consistency
- **Marker validation** — FFOC, LFOC, FFMC, LFMC presence (strict mode)
- **Cross-reference integrity** — All PKL/CPL asset references resolve
- **Supplemental DCP** — Original Package List validation

### Advanced Tools
- **DCP comparison/diff** — Side-by-side structural comparison of two DCPs
- **Theater compatibility profiles** — Pre-built profiles for major server vendors:
  - Dolby IMS3000, IMS2000, Cinema (Premium)
  - Barco SP4K, SP2K
  - Christie CP4440-RGB, CP2230
  - GDC SX-4000, SR-1000
  - IMAX Digital
- **Automated fix suggestions** — Actionable remediation advice for common issues
- **SVG timeline visualization** — Visual reel structure diagram with timecodes
- **Manifest comparison** — Validate DCP against a reference manifest JSON
- **Content hash cache** — SQLite-backed cache for instant re-validation of unchanged files
- **Batch processing** — Multi-DCP validation with summary table

### Output & Integration
- **Colored terminal output** — ANSI colors (respects `NO_COLOR` and non-TTY)
- **Progress bar** — Visual progress for batch operations
- **Text/JSON/HTML reports** — Multiple output formats
- **REST API** — HTTP server mode (POST /validate, GET /health)
- **Directory watch** — Auto-validates new DCPs as they appear
- **Exit codes** — Machine-parseable pass/fail status

## Installation

### Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| CMake | 3.25+ | Build system |
| C++ compiler | GCC 13+ / Clang 17+ | C++23 support required |
| libxml2 | 2.12+ | XML parsing, C14N, XPath |
| OpenSSL | 3.0+ | SHA hashes, RSA signatures, X.509 |
| Xerces-C++ | 3.2+ | XML schema validation |
| SQLite3 | 3.30+ | Hash cache persistence |

#### Bundled (git submodules)
| Library | Purpose |
|---|---|
| [CLI11](https://github.com/CLIUtils/CLI11) | Command-line parsing |
| [spdlog](https://github.com/gabime/spdlog) | Structured logging |
| [asdcplib](https://github.com/cinecert/asdcplib) | MXF/J2K/PCM essence reading |

### Build

```bash
git clone --recurse-submodules https://github.com/youruser/dcpdoctor.git
cd dcpdoctor
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

If submodules weren't cloned:
```bash
git submodule update --init --recursive
```

### Install (system-wide)

```bash
sudo make install
```

### Fedora/RHEL

```bash
sudo dnf install cmake gcc-c++ libxml2-devel openssl-devel xerces-c-devel sqlite-devel
```

### Ubuntu/Debian

```bash
sudo apt install cmake g++ libxml2-dev libssl-dev libxerces-c-dev libsqlite3-dev
```

### macOS (Homebrew)

```bash
brew install cmake libxml2 openssl xerces-c sqlite
```

## Usage

### Basic Validation

```bash
# Validate a DCP
dcpdoctor /path/to/dcp

# Verbose output (shows INFO notes)
dcpdoctor -v /path/to/dcp

# Quiet mode (errors only)
dcpdoctor -q /path/to/dcp

# Multiple DCPs (shows batch summary)
dcpdoctor /dcp1 /dcp2 /dcp3
```

### Standards & Compliance

```bash
# BV2.1 application profile check
dcpdoctor --bv21 /path/to/dcp

# Strict SMPTE compliance
dcpdoctor --strict /path/to/dcp

# Deep J2K codestream validation
dcpdoctor --deep-j2k /path/to/dcp

# MXF essence inspection (bitrate, audio levels)
dcpdoctor --check-mxf /path/to/dcp
```

### Reports & Output

```bash
# JSON report
dcpdoctor --json /path/to/dcp

# HTML report
dcpdoctor --html -o report.html /path/to/dcp

# SVG timeline visualization
dcpdoctor --timeline timeline.svg /path/to/dcp

# Fix suggestions
dcpdoctor --fix /path/to/dcp
```

### DCP Comparison

```bash
# Compare two DCPs
dcpdoctor diff /path/to/dcp_v1 /path/to/dcp_v2

# Include content hash comparison (slower)
dcpdoctor diff --hashes /path/to/dcp_v1 /path/to/dcp_v2
```

### KDM Validation

```bash
# Validate KDM file
dcpdoctor kdm /path/to/kdm.xml

# Validate KDM against specific DCP
dcpdoctor kdm /path/to/kdm.xml --dcp /path/to/dcp
```

### Theater Profiles

```bash
# List all built-in theater profiles
dcpdoctor profiles

# Check DCP against specific theater
dcpdoctor profiles --check "dolby ims3000" --dcp /path/to/dcp
dcpdoctor profiles --check "imax" --dcp /path/to/dcp
```

### Manifest Comparison

```bash
# Compare DCP against reference manifest
dcpdoctor --manifest manifest.json /path/to/dcp
```

Manifest JSON format:
```json
{
  "assets": [
    {"filename": "picture.mxf", "size": 1234567890},
    {"filename": "sound.mxf", "size": 987654321}
  ]
}
```

### Server & Watch Modes

```bash
# REST API server
dcpdoctor serve --port 8080

# Auto-validate new DCPs in a directory
dcpdoctor watch /ingest/incoming --interval 5000
```

REST API endpoints:
- `GET /health` — Returns `{"status": "ok"}`
- `POST /validate` — Body: `{"path": "/path/to/dcp"}`, returns validation result

### Performance Options

```bash
# Skip hash verification (fast structural check only)
dcpdoctor --no-hashes /path/to/dcp

# Skip signature verification
dcpdoctor --no-signatures /path/to/dcp
```

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | All DCPs passed validation |
| `1` | One or more DCPs failed |
| `2` | Usage/configuration error |

## Environment Variables

| Variable | Effect |
|---|---|
| `NO_COLOR` | Disable colored output |

## Running Tests

```bash
cd build
./dcpdoctor_test
# 135/135 tests passed
```

## Architecture

```
dcpdoctor/
├── include/dcpdoctor/    # Public headers
│   ├── dcpdoctor.h       # Core types (Note, Code, VerifyResult, VerifyOptions)
│   ├── advanced.h        # BV2.1, manifest, batch
│   ├── audio.h           # Audio level analysis
│   ├── bitrate.h         # J2K bitrate stats
│   ├── cache.h           # SQLite hash cache
│   ├── diff.h            # DCP comparison
│   ├── fixes.h           # Fix suggestions
│   ├── kdm.h             # KDM parsing/validation
│   ├── report.h          # Output formatting + progress bar
│   ├── server.h          # REST API + watch
│   ├── theater.h         # Theater compatibility profiles
│   └── timeline.h        # SVG timeline + audio sync
├── src/                  # Implementation
├── tests/                # Unit tests (135 assertions)
└── extern/               # Git submodules (CLI11, spdlog, asdcplib)
```

## License

MIT
