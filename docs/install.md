# Install swarm-adapter-ns3

<!--
The org's install guide (swarm-routing/.github CONTRIBUTING "Class
documentation and install guide"), from its templates/install/install.md.in.
Every command here is one that runs today; the "Known simulator bugs" section
is org-managed (generated from swarm-routing/sim-tools docs/ns3-bugs.md by
swarm-routing/.github cmd/ns3-bugs, refreshed by org-bot's org-sync PRs):
never edit it here.
-->

This repository ships one thing: the ns-3 contrib module **`swarm-core`**
(the name predates the repository's), the ns-3 adapter of
[swarm-routing/swarm-core](https://github.com/swarm-routing/swarm-core)'s
ports that every swarm-routing protocol's ns-3 module links. Install it in
one of three ways:

- into **your own ns-3 tree**, from the release asset (the usual way; see
  [ns-3](#ns-3));
- through **vcpkg**, as the port `swarm-adapter-ns3` on the `ns3` and
  `swarm-core` ports (see [Linux through vcpkg](#linux-through-vcpkg-manifest-mode));
- as the **container image** `ghcr.io/swarm-routing/swarm-adapter-ns3:v<ver>-{debug,optimized}`
  (or `latest-debug`, `latest-optimized`), ns-3 with this module already built.

Its simulator-free primitives (`swarm::core`) are swarm-core's: install them
without ns-3 from that repository. The class documentation is
[docs/api/](api/mainpage.md), published at
<https://swarm-routing.github.io/swarm-adapter-ns3/api/>.

## Supported platforms

| Part | Platform | Compiler, C++ standard | CMake | Simulator |
|------|----------|------------------------|-------|-----------|
| the ns-3 module `swarm-core` | Linux x64 (CI: Ubuntu 24.04 in the `ns3-dev` image) | C++23 (GCC 13.3 in CI) | 3.28 in CI | ns-3.46 or later; CI builds ns-3.49 on every PR, ns-3.48 on demand |
| the vcpkg port `swarm-adapter-ns3` | `x64-linux` only | C++23 | the vcpkg toolchain | the `ns3` port (ns-3.49) |

C++23 is the standard of the ns-3 release swarm-core supports (ns-3's own
minimum from 3.46; [api-policy.md](api-policy.md#c-standard)). The module's
test suite needs ns-3.42 or later; the C++23 primitives need 3.46 or later.

## Linux through vcpkg (manifest mode)

Each release opens its port bump in the org's registry,
[swarm-routing/vcpkg-registry](https://github.com/swarm-routing/vcpkg-registry)
(`vcpkg-ports: swarm-adapter-ns3` in [release.yml](../.github/workflows/release.yml)).
The port builds this module against the registry's `ns3` port (stock ns-3.49
plus the upstream fix for NS3-001, see [Known simulator bugs](#known-simulator-bugs))
and the `swarm-core` port. Install vcpkg once:

```bash
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"
```

In your project, `vcpkg.json`:

```json
{
  "name": "my-simulation",
  "version": "0.1.0",
  "dependencies": ["swarm-adapter-ns3"]
}
```

`vcpkg-configuration.json` takes the three org ports from the registry and
vcpkg's helper ports from the builtin one. Each `baseline` is a commit of
that registry: the builtin one is your vcpkg checkout's
(`git -C "$VCPKG_ROOT" rev-parse HEAD`), the org's is a commit of its `main`
(`git ls-remote https://github.com/swarm-routing/vcpkg-registry refs/heads/main`;
`c0a113d5bc7cc214288a26b3c865543555843ebe` holds `swarm-adapter-ns3` 2.1.0,
`swarm-core` 2.0.0 and `ns3` 3.49):

```json
{
  "default-registry": {
    "kind": "builtin",
    "baseline": "<your vcpkg commit>"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/swarm-routing/vcpkg-registry",
      "baseline": "c0a113d5bc7cc214288a26b3c865543555843ebe",
      "packages": ["ns3", "swarm-core", "swarm-adapter-ns3"]
    }
  ]
}
```

`CMakeLists.txt` (the port's `usage`):

```cmake
cmake_minimum_required(VERSION 3.28)
project(my-simulation CXX)
find_package(swarm-adapter-ns3 CONFIG REQUIRED)
add_executable(my-simulation main.cc)
target_compile_features(my-simulation PRIVATE cxx_std_23)
target_link_libraries(my-simulation PRIVATE ns3::swarm-core)
```

Configure with the vcpkg toolchain, which builds and installs the manifest's
ports first (the `ns3` port builds ns-3 itself, so the first configure takes
a while):

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build -j
```

`main.cc` includes `"ns3/swarm-core-module.h"` (the glue and swarm-core's
primitives) and the ns-3 modules it uses (`"ns3/core-module.h"`, ...).

## ns-3

Into your own ns-3 tree, from the release asset `swarm-core-<ver>.tar.gz`
(its top directory is `swarm-core/`; check it against the release's
`SHA256SUMS`). The configure step downloads the swarm-core release
[`swarm-core.lock`](../swarm-core.lock) names and checks its SHA256, so it
needs network access once; `-DSWARM_CORE_DIR=<swarm-core checkout>/core`
uses a local copy instead.

### ns-3.49

CI builds and tests the module on ns-3.49 on every pull request (the
`ns3-dev` image's `latest`, debug and sanitized).

- **Required tools**: a C++23 compiler (GCC 13 or later; CI: GCC 13.3),
  CMake (CI: 3.28), Python 3 (for `./ns3`), Git, and network access for the
  first configure.
- **Optional tools**: Ninja (`-G Ninja`, faster builds), `ccache`, Docker
  (the images below, nothing to build), vcpkg (above).
- **Install** (release v2.1.0 here; take the newest from
  [the releases](https://github.com/swarm-routing/swarm-adapter-ns3/releases)):

```bash
git clone --depth 1 --branch ns-3.49 https://gitlab.com/nsnam/ns-3-dev.git ns-3
curl -LO https://github.com/swarm-routing/swarm-adapter-ns3/releases/download/v2.1.0/swarm-core-2.1.0.tar.gz
curl -LO https://github.com/swarm-routing/swarm-adapter-ns3/releases/download/v2.1.0/SHA256SUMS
sha256sum --check --ignore-missing SHA256SUMS && tar -xzf swarm-core-2.1.0.tar.gz -C ns-3/contrib
cd ns-3
./ns3 configure --enable-examples --enable-tests && ./ns3 build
./test.py -s swarm-core
./ns3 run "swarm-core-seed --seed=1"
```

  `--enable-modules=swarm-core` on the configure line builds only this module
  and the ns-3 modules it needs, instead of all of ns-3.
- **Or the container**:
  `docker run --rm -it ghcr.io/swarm-routing/swarm-adapter-ns3:v2.1.0-debug`
  (or `-optimized`) is ns-3 with this module built. To build your own module against it, or
  against the bare `ghcr.io/swarm-routing/ns3-dev:latest-debug`, mount it and
  run `ns3-build-contrib` as this repository's `make build` does
  ([development.md](development.md#build)).
- **A protocol module** then links it, `LIBRARIES_TO_LINK ${libswarm-core}`,
  and includes `"ns3/swarm-core-module.h"`; swarm-core's include directory
  and C++ standard come with the link (README, "swarm-core's primitives: a
  PUBLIC dependency").
- **Known bugs on this release**: NS3-001 and NS3-002 affect ns-3.49, and
  NS3-003 is in its DSDV source (see the table below). All three are in the
  stock arms protocols compare against (AODV, DSDV), not in this module's
  code; a protocol run beside those arms takes the workaround listed for
  each.

### ns-3.48

CI builds ns-3.48 on demand (`ci.yml` dispatched with
`ns3-versions: ["3.48"]`), on the `3.48-debug` image.

- **Required and optional tools**: as for ns-3.49.
- **Install**: the same commands, with `--branch ns-3.48`; or the image
  `ghcr.io/swarm-routing/ns3-dev:3.48-debug`.
- **Known bugs on this release**: NS3-002 and NS3-003; and four wifi unit
  suites fail when `test-runner` is built without the `test` module (fixed
  in 3.49; see "Fixed upstream" below). NS3-001's livelock is not in 3.48.

## OMNeT++

Not supported: this repository is the ns-3 adapter. The OMNeT++ adapter is
[swarm-routing/swarm-adapter-omnet](https://github.com/swarm-routing/swarm-adapter-omnet).

## Known simulator bugs

<!-- org-sync:begin known-bugs -->
<!-- org-sync:end known-bugs -->

## Troubleshooting

- **The configure step fails to download swarm-core, or reports a hash
  mismatch.** The tree has no network access, or the asset is not the one
  `swarm-core.lock` names. Build offline with
  `-DSWARM_CORE_DIR=<swarm-core checkout>/core`; never edit the lock by hand.
- **CMake reports that C++23 is not supported, or a protocol module fails on
  C++23 code.** The compiler or the ns-3 release is too old: GCC 13 or later,
  ns-3.46 or later.
- **A protocol's ns-3 module fails with `contrib/swarm-core/core ... not
  found`.** It still looks for the `core/` this repository no longer ships
  (#231): drop that path; linking `${libswarm-core}` brings swarm-core's
  include directory.
- **vcpkg: `swarm-adapter-ns3` is not found, or has no such version.** The
  registry is missing from `vcpkg-configuration.json`, the port is not in its
  `packages` list, or the `baseline` predates the version: move the baseline
  to the registry's `main`.
- **vcpkg on another triplet.** The `ns3` and `swarm-adapter-ns3` ports are
  `x64-linux` only.
- More, for working on this repository rather than installing it:
  [development.md](development.md#troubleshooting).
