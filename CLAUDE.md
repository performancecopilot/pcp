# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Performance Co-Pilot (PCP) is a mature, extensible, cross-platform toolkit for system-level performance monitoring and management. It provides a unifying abstraction for all performance data in a system and many tools for interrogating, retrieving and processing that data.

## Development Commands

### Building and Packaging
```bash
# Configure and build from source (requires autotools)
./configure --prefix=/usr --libexecdir=/usr/lib --sysconfdir=/etc --localstatedir=/var
make

# Build packages for the current platform
./Makepkgs --verbose

# Cross-compile for Windows (requires MinGW)
./Makepkgs --verbose --target mingw64

# Install manually after building
sudo make install
```

### Quality Assurance Testing
```bash
# Run QA setup validation
qa/admin/check-vm

# Install dependencies for your platform
qa/admin/list-packages -m

# Run all QA tests
cd qa && ./check

# Run specific tests
cd qa && ./check 000        # Run test 000
cd qa && ./check 100-200    # Run tests 100-200
cd qa && ./check -g pmcd    # Run pmcd group tests

# Create new test
cd qa && ./new

# Remake expected output for a test
cd qa && ./remake 123
```

### PMCD Service Management
```bash
# Start the PCP daemon
sudo systemctl start pmcd
# or
sudo service pmcd start

# Check PCP metrics
pminfo -dfmt
pmprobe -v
```

## High-Level Architecture

### Core Components

- **libpcp**: Main PCP library providing core functionality
- **pmcd**: Performance Metrics Collection Daemon - central coordinator
- **pmlogger**: Performance data logging daemon  
- **pmie**: Performance Metrics Inference Engine - rule-based monitoring
- **pmproxy**: Web API proxy and time series interface

### Tool Categories

1. **Data Collection Tools**: pmcd, pmlogger, various pmdas
2. **Data Export Tools**: pcp2arrow, pcp2elasticsearch, pcp2json, etc.
3. **Analysis Tools**: pminfo, pmval, pmstat, pmchart, pmrep and pcp subtools
4. **Archive Tools**: pmlogdump, pmlogextract, pmlogrewrite, pmlogsummary
5. **Administrative Tools**: pmconfig, pmlc, pmafm, pmcheck

### PMDAs (Performance Metrics Domain Agents)

PMDAs collect metrics from specific subsystems. Key directories in `src/pmdas/`:

- **Platform PMDAs**: linux, darwin, aix, windows - core system metrics
- **Application PMDAs**: apache, mysql, postgres, mongodb, elasticsearch
- **System PMDAs**: bpf, bpftrace, nvidia, amdgpu
- **Network PMDAs**: cisco, bind2, apache, nginx
- **Storage PMDAs**: dm, nfsclient, gluster, xfs
- **Development PMDAs**: sample, simple, trivial - for testing and examples

### Library Structure

- **libpcp**: Core library with platform abstractions
- **libpcp_pmda**: PMDA development framework
- **libpcp_pmcd**: PMCD-specific functionality
- **libpcp_gui**: GUI toolkit integration
- **libpcp_web**: Web API and time series support
- **libpcp_trace**: Event trace instrumentation
- **libpcp_import**: Data import for PCP archives

## Development Guidelines

### QA Testing Philosophy
- All changes should include appropriate QA tests
- Tests should be deterministic and portable across platforms
- Use existing archives and filtering functions where possible
- Tests run as non-root user with sudo for privileged operations

### PMDA Development
- Use `src/pmdas/simple` or `src/pmdas/sample` as starting templates
- Follow the Install/Remove script patterns in existing PMDAs
- PMDAs can be written in C, Python, or Perl

### Code Conventions
- Follow existing code style in surrounding files
- Add permanent diagnostics using pmDebugOptions framework
- Use GPL-compatible licensing for all contributions
- Never introduce code that logs or exposes secrets

## Testing Requirements

### Prerequisites
- PCP must be installed (not run from source tree)
- User "pcp" must exist for services
- User "pcpqa" used for testing via pcp-testsuite package
- Sudo access required for privileged operations

### Essential QA Commands
```bash
# First-time setup validation
cd qa && ./check 000

# Check specific functionality
cd qa && ./check -g pmcd    # PMCD tests
cd qa && ./check -g pmda    # PMDA tests  
cd qa && ./check -g archive # Archive tests
```

### Platform Support
PCP supports Linux, macOS, Windows (MinGW), AIX, and Solaris. Tests should be written to be portable or use `_notrun()` for platform-specific limitations.
