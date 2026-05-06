# dcplint

A fast, modern DCP (Digital Cinema Package) validator and verifier.

## Features

- **Structure validation**: ASSETMAP, PKL, CPL parsing and cross-referencing
- **Schema validation**: XML validation against SMPTE/Interop schemas
- **Signature verification**: X.509 certificate and XML digital signature checks
- **Compliance checking**: SMPTE ST 429 and Interop DCP standards
- **Multiple output formats**: text, JSON, HTML reports
- **Fast**: C++23 with minimal dependencies

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Dependencies

- CMake 3.25+
- C++23 compiler (GCC 13+ or Clang 17+)
- libxml2
- OpenSSL
- Xerces-C++

## Usage

```bash
# Basic validation
dcplint /path/to/dcp

# JSON output
dcplint --json /path/to/dcp

# Skip expensive checks
dcplint --no-hashes --no-signatures /path/to/dcp

# Strict SMPTE mode
dcplint --strict /path/to/dcp

# Multiple DCPs
dcplint /dcp1 /dcp2 /dcp3
```

## Exit Codes

- `0` — All DCPs passed validation
- `1` — One or more DCPs failed
- `2` — Usage error

## Roadmap

- [ ] Full hash verification (SHA-1 for asset integrity)
- [ ] MXF file inspection (via asdcplib)
- [ ] Picture bitstream validation (resolution, frame rate, JPEG2000 profile)
- [ ] Sound validation (sample rate, channel count, channel layout)
- [ ] KDM support for encrypted content
- [ ] DCDM subtitle/caption validation
- [ ] BV 2.1 (SMPTE Bv2.1) compliance profile
- [ ] HTML report generation with visual layout

## License

MIT
