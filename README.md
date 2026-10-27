# samp-profiler

![Build](https://github.com/DeusExMachinaaa/samp-profiler/actions/workflows/ci.yml/badge.svg)

An intentionally small, low-overhead profiler for SA-MP and open.mp servers.
It measures the sections you choose, keeps statistics in memory, and stays
silent until you explicitly request a report.

## Why this profiler is lightweight

The hot path does not allocate memory, lock a mutex, write a file, or print a
log line. A section is registered once and measured by an integer ID. Results
are aggregated in fixed-size storage and reported on demand.

Each section records:

- total calls;
- average duration;
- maximum duration; and
- an approximate p95 duration using a 32-bucket logarithmic histogram.

The first release uses explicit instrumentation. This is a deliberate safety
choice: SA-MP does not expose a stable public hook for wrapping every Pawn
callback automatically, while AMX debug hooks can add substantial per-
instruction overhead.

## Repository layout

```text
plugin/                 Pawn include distributed to server owners
source/                 C++ plugin implementation and bundled SDK
tests/                  Standalone core tests
.github/workflows/      CI and release automation
README.md               This guide
```

## Installation

Download the release asset for your platform from the GitHub Releases page.
Copy the matching binary to the server's `plugins` directory:

- SA-MP 0.3.x: use the 32-bit build (`samp-profiler.dll` or `.so`).
- open.mp: use the 64-bit build when running a 64-bit server.

Copy [`plugin/samp_profiler.inc`](plugin/samp_profiler.inc) to your Pawn
include directory, then load the plugin in `server.cfg`:

```text
plugins samp-profiler
```

On Linux, use the `.so` suffix. On Windows, use the `.dll` suffix. The plugin
does not create files during normal profiling.

## Pawn usage

```pawn
#include <samp_profiler>

new g_profile_update;

public OnGameModeInit()
{
    g_profile_update = SP_Register("OnPlayerUpdate");
    SP_SetEnabled(true);
    return 1;
}

public OnPlayerUpdate(playerid)
{
    new scope = SP_Begin(g_profile_update);

    // Code to measure.

    SP_End(scope);
    return 1;
}
```

`SP_End` is silent and returns `1` when the scope is closed successfully. It
does not print a report. Request a report only when needed:

```pawn
SP_Dump(100); // show sections whose average or maximum is at least 100 us
```

Example server output:

```text
[samp-profiler] sections=1 enabled=1
[samp-profiler] #1 OnPlayerUpdate calls=12500 avg=42us p95~=64us max=310us
```

`us` means microseconds: `1 ms = 1,000 us`.

Sections may be nested up to 32 levels per AMX. `SP_End` must be called in the
reverse order of `SP_Begin`.

## C++ usage

Include [`source/include/samp_profiler_api.h`](source/include/samp_profiler_api.h)
from a native plugin and use the RAII helper:

```cpp
#include "samp_profiler_api.h"

const auto section = SP_RegisterSection("database-refresh");
{
    SampProfilerScope scope(section);
    // Work to measure.
}
```

The first release expects profiling calls to happen on the server's main
thread. This keeps the hot path lock-free and predictable.

## Build from source

The project bundles the SA-MP Plugin SDK, so a fresh checkout is self-contained.

### Legacy SA-MP Linux build (32-bit)

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ g++-multilib

cmake -S . -B build-samp-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DSAMP_PROFILER_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-m32" \
  -DCMAKE_SHARED_LINKER_FLAGS="-m32"

cmake --build build-samp-linux -j$(nproc)
ctest --test-dir build-samp-linux --output-on-failure
```

### open.mp Linux build (64-bit)

```bash
sudo apt-get update
sudo apt-get install -y cmake g++

cmake -S . -B build-openmp-linux \
  -DCMAKE_BUILD_TYPE=Release \
  -DSAMP_PROFILER_BUILD_TESTS=ON

cmake --build build-openmp-linux -j$(nproc)
ctest --test-dir build-openmp-linux --output-on-failure
```

Windows builds are generated with CMake's `Win32` generator for SA-MP and the
`x64` generator for open.mp. GitHub Actions builds and tests all four targets
automatically.

## Releases

- `v1.0.0-samp` — legacy SA-MP 32-bit targets.
- `v1.0.0-openmp` — open.mp 64-bit targets.

The release workflows build the binaries, run the tests, package the include
and documentation, and publish the resulting assets to GitHub Releases.
