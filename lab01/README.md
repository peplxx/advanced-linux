# BLDD - Backward LDD

**BLDD** (Backward LDD) is a command-line utility that finds executable files depending on specified shared libraries. Unlike the traditional `ldd` command which shows dependencies of a single binary, BLDD scans directories to discover which executables link against given libraries.

## Features

- Scan directories for executables using specific shared libraries
- Support for multiple architectures: x86, x86_64, ARM, AArch64
- Cross-platform support for ELF (Linux) and Mach-O (macOS) binaries
- Filter by library name (partial matching)
- Filter by architecture and binary format
- Configurable output formats
- Verbose and recursive scanning options
- Configuration file support
- Binary symlinks handling

## Installation
<details>
<summary>From Releases</summary>

### From Release
Download the latest prebuilt binary from the [releases page](https://github.com/peplxx/bldd/releases):

#### Linux (x86_64)
```bash
# Download and extract
curl -L https://github.com/peplxx/bldd/releases/download/v0.2.2/bldd-linux-x86_64.tar.gz | tar xz

# Make it executable
chmod +x bldd

# Move to PATH (optional)
sudo mv bldd /usr/local/bin/
```

#### Macos (Intel)
```bash
# Download and extract
curl -L https://github.com/peplxx/bldd/releases/download/v0.2.2/bldd-macos-x86_64.tar.gz | tar xz

# Make it executable
chmod +x bldd

# Move to PATH (optional)
sudo mv bldd /usr/local/bin/
```

#### Macos (Apple Silicon)
```bash
# Download and extract
curl -L https://github.com/peplxx/bldd/releases/download/v0.2.2/bldd-macos-aarch64.tar.gz | tar xz

# Make it executable
chmod +x bldd

# Move to PATH (optional)
sudo mv bldd /usr/local/bin/
```


</details>
<details>
<summary>From Sources</summary>

### Requirements

- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.16+
- Git

### Steps

```bash
# Clone the repository
git clone https://github.com/peplxx/bldd.git
cd bldd

# Configure and build
cmake -B build
cmake --build build

ln -s $(pwd)/build/bin/bldd ~/.local/bin/bldd
```

</details>

## Quick Example

Find all executables in `/usr/bin` that depend on OpenSSL libraries:

```bash
bldd -d /usr/bin -l libssl -l libcrypto
```
### Output:
```
libcrypto.46.dylib [x86_64] (31 execs):
    libcrypto.46.dylib -> /usr/bin/ssh
    libcrypto.46.dylib -> /usr/bin/scp
    libcrypto.46.dylib -> /usr/bin/sftp
    libcrypto.46.dylib -> /usr/bin/ssh-keygen
    libcrypto.46.dylib -> /usr/bin/ssh-add
    libcrypto.46.dylib -> /usr/bin/ssh-agent
    libcrypto.46.dylib -> /usr/bin/ssh-keyscan
    libcrypto.46.dylib -> /usr/bin/openssl
    libcrypto.46.dylib -> /usr/bin/ocspcheck
    ... (22 more executables)

libssl.48.dylib [x86_64] (4 execs):
    libssl.48.dylib -> /usr/bin/openssl
    libssl.48.dylib -> /usr/bin/ocspcheck
    libssl.48.dylib -> /usr/bin/newaliases
    libssl.48.dylib -> /usr/bin/mailq

----------------------------------------
libs total: 2
execs total: 31
```

## Usage

```bash
bldd [OPTIONS]
```

### Basic Examples

```bash
# Find all executables using libc in /usr/bin
bldd -d /usr/bin -l libc

# Find executables using libssl or libcrypto, output to file
bldd -d /usr/bin -l libssl -l libcrypto -o report.txt

# Scan multiple directories for x86_64 binaries only
bldd -d /usr/bin -d /usr/local/bin --arch x86_64

# Scan with verbose output
bldd -d /usr/bin -v

# Scan without recursion
bldd -d /opt --no-recursive

# Scan without symlinks
bldd -d /usr/bin --no-symlinks
```

### Options

| Option | Description |
|--------|-------------|
| `-d, --directory` | Directory to scan for executables (multiple allowed) |
| `-l, --library` | Library filter to search for (multiple allowed, partial match, all allowed) |
| `-a, --arch` | Filter by architecture: x86, x86_64, armv7, aarch64 |
| `-o, --output` | Output file for the report (default: stdout) |
| `--bformat` | Filter by binary format: `elf`, `macho`, `all` |
| `-c, --config` | Path to configuration file |
| `-v, --verbose` | Enable verbose output |
| `--no-recursive` | Do not scan directories recursively |
| `--no-symlinks` | Do not scan symlinks in directories |
| `--examples` | Show usage examples |

## Configuration File

BLDD supports [configuration files](./config/bldd.conf.example) for persistent settings.

By default, BLDD looks for a configuration file at `~/.config/bldd/bldd.conf`.

You can override this with the `--config` flag.

## Testing

BLDD includes unit and end-to-end tests:

<details>
<summary>Running tests</summary>

```bash
# Build with tests
cmake -B -DBUILD_TESTING=ON build
cmake --build build

# Run unit tests
./build/bin/tests

# Run end-to-end tests
cmake -B -DBUILD_TESTING=ON -DBUILD_E2E=ON build
cmake --build build

./build/bin/tests

# Run cross-platform end-to-end script
./scripts/run-e2e.sh
```

</details>

## Playground

A Docker-based [playground](./playground/README.md) environment is available for testing:

<details>
<summary>Using the playground</summary>

```bash
# Start the playground
docker-compose -f playground/docker-compose.yaml up -d
docker-compose -f playground/docker-compose.yaml exec playground bash
```

</details>

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Bug Reports

Report bugs at: https://github.com/peplxx/bldd/issues