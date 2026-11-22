# AGENTS.md — guide for AI coding agents

Operational guide for LLM agents working in `swarm-routing/ns3-swarm-core`. Read this
before changing anything. Org-wide conventions:
[`swarm-routing/.github` CONTRIBUTING.md](https://github.com/swarm-routing/.github/blob/main/CONTRIBUTING.md). Shape and rules mirror
[AntHocNet's AGENTS.md](https://github.com/swarm-routing/AntHocNet/blob/main/AGENTS.md), the source repository.

## What this repo is

**Status: primitives in `core/` (#8), ns-3 glue in `model/` and `helper/` (#9), node lifecycle (#43).** CI runs every org quality gate on the module. v1.0.0 froze the API (#83, `docs/api-policy.md`): API version 2 (`kSwarmCoreApiVersion`) for all of 1.x.

The shared ns-3 contrib module `swarm-core` (`libns3.<ver>-swarm-core`) every
swarm-routing protocol links against: simulator-agnostic C++23 primitives
(`VisitedPath`, little-endian wire helpers, `PheromoneTable`, `RouteDecision`,
`ILinkState`, `ILinkMetric`, evaporation and roulette selection) extracted from
AntHocNet's dual-licensed `core/`, plus ns-3 cross-layer glue
(`SwarmRoutingBase`, `DeferredPacketQueue`, `WifiLinkMonitor`,
`EnergyMetricHook` over `ns3::EnergySource`, `IslLinkMonitor`).

```
CMakeLists.txt      build_lib(LIBNAME swarm-core ...); ns-3 generates ns3/swarm-core-module.h
core/               simulator-free primitives, namespace swarm::core — MIT OR GPL-2.0-only, carries core/LICENSE:
                    swarm-types.h, swarm-ports.h (IClock, IRng, INeighborProvider, ITimerScheduler with
                    cancellable timers, ILinkState, ILinkQuality, IEnergy), swarm-wire.h (LE WireWriter,
                    FixedWireWriter<N> with no allocation, WireReader; version byte; #46),
                    swarm-visited-path.h, swarm-dedup-history.{h,cc} (count cap, optional entry lifetime;
                    #33), swarm-pheromone-table.{h,cc} (flat per-destination rows: row() views, forEach,
                    bestEntry; #45),
                    swarm-selection.{h,cc} (roulette, evaporationFactor), swarm-route-decision.h,
                    swarm-link-metric.h (ILinkMetric; LinkObservation::stability, #3), swarm-energy-metric.h
                    (EnergyMetric: 1 / residual energy per hop, floored; AntHocNet's bit for bit; #2),
                    swarm-fuzzy-metric.h (FuzzyMetric: delay x stability rule table, quality / hops; #3),
                    swarm-observer.{h,cc} (IRouterObserver, KindName,
                    CountingObserver -> RouterStats: bounded counters); CMakeLists.txt builds it alone, no ns-3
                    swarm-core-sources.cmake: the files of core/, listed once for both builds (#100)
                    swarm-coreConfig.cmake.in: the installed CMake package swarm-core (find_package; imported
                    swarm::core; version from VERSION, SameMajorVersion from 1.0, SameMinorVersion before; #133)
core/testbench/     swarm-testbench.h: the discrete-event testbench for protocol cores' tests (#15; header-only,
                    MIT OR GPL-2.0-only): EventQueue (bounded, cancellable), Rng (SplitMix64 streams), Topology
                    (links, partition), Network<Msg> (ideal MAC, linkAt, setLoss, counters, bounded trace + digest),
                    INodeAdapter<Msg> / NodePorts<Msg>; not in the ns-3 module's headers; README "Test a protocol core"
core/test/          the core tests, one function per primitive (MIT OR GPL-2.0-only); run by ctest and by test.py; test-testbench.cc: the bench's tests and the reference trend
                    (local repair vs re-flood on a 6-node line with one break)
core/test/consumer/ a scratch CMake project that links swarm::core, built by tools/checks/check-core-consumer.sh (#100)
core/test/consumer-package/  the same consumer on a released swarm-core: find_package against an install prefix, or
                    FetchContent of the release tarball by URL + URL_HASH (#133, #134); built by the same script
core/bench/         dev-only microbenchmarks (MIT OR GPL-2.0-only; Google Benchmark, pinned, -DSWARM_CORE_BENCH=ON;
                    exempt from the core-include check, as it includes <benchmark/benchmark.h>)
model/              ns-3 glue — GPL-2.0-only; each file derived from AntHocNet names its source file and commit:
                    swarm-core.h (kSwarmCoreApiVersion, SwarmCoreSeed), swarm-ns3-ports.h (SwarmClock, SwarmRng,
                    SwarmTimerScheduler, address conversion, null ports), swarm-deferred-packet-queue.h
                    (DeferredPacketQueue: MaxPackets 64 / MaxAge 30 s attributes, drop accounting, in-place
                    moves, Requeue in one batch; #46; hold classes with a cap and statistics each,
                    RearmOnEnqueue; #126), swarm-wire-buffer.h (BufferWireWriter, ReadWireBytes:
                    the core wire format straight into and out of an ns-3 Buffer, #46),
                    swarm-routing-base.h (SwarmRoutingBase: sockets, deferred route, prev-hop tag, ports, Execute;
                    Observer() -> ControlTx/ControlRx/RouteChanged/DataDecision/StateSize trace sources,
                    GetRouterStats, the kind-name registry SetKindNames/GetKindName; node lifecycle #43:
                    Shutdown/Crash/Start, the DoNodeShutdown/DoNodeCrash/DoNodeStart hooks, the Lifecycle
                    trace source, NodeDown drops; neighbours from link state on p2p #4: IslHelloSuppression,
                    BroadcastBeacon, BeaconSuppressed, IsLinkStateNeighbor, peers announced through OnLinkUp;
                    the peer map #127: SetPeerRoute/ClearPeerRoute, next hops resolved as AntHocNet's),
                    swarm-wifi-link-monitor.h (ILinkState + ILinkQuality + retry-limit LinkFailure),
                    swarm-isl-link-monitor.h (INeighborProvider + ILinkState + LinkChange on p2p; PeerOnInterface,
                    IsAuthoritative: one known peer, never failed, TrustAfterFailure off, #4),
                    swarm-energy-metric-hook.h (IEnergy from an EnergySource's RemainingEnergy trace)
helper/             swarm-routing-helper.h: SwarmRoutingHelper (one-call Install, AssignStreams, SetKindNames,
                    CollectStats -> SwarmRoutingStats), SwarmMonitorHelper; swarm-lifecycle-helper.h:
                    SwarmLifecycleHelper (shutdown, crash, start at given times or from a schedule file, the
                    protocol's lifecycle plus Ipv4 SetDown/SetUp, so stock arms see the same failure; #43)
test/               swarm-core test suite: wraps every core/test/ function as a TestCase, plus the glue's
                    (swarm-glue-tests.cc: a toy protocol on the base, a 2-3 node scenario per monitor, and
                    the router observer's trace sources and counters, same-seed byte-identical output
                    with and without every monitor and the observer)
examples/           swarm-core-seed: the determinism gate's program (seeded draws + a roulette sequence through IRng);
                    swarm-core-queue-bench: dev-only drain cost of the deferred queue (#46), not run by CI
tools/checks/       check-core-consumer.sh: core/ in another CMake project gets the library alone (no test target, no ctest
                    entry, no -Werror of ours), installs as the package find_package finds (its version rule
                    included), and its dist.sh tarball builds by URL + URL_HASH; run by the `core` check;
                    check-cxx-standard.py: core/'s C++ standard against an ns-3 tree's minimum (#141);
                    check-api-surface.py: docs/api-surface.txt, the generated record of the public API, and the guard that
                    a PR removing or changing an item in it is marked breaking (#83) — MIT OR GPL-2.0-only
tools/bench/        bench_compare.py: the PR-vs-base microbenchmark table and its +15 % warning (#170) — MIT OR GPL-2.0-only
tools/release/      dist.sh: the release asset (swarm-core-<ver>.tar.gz), checked in a clean ns3-dev tree; `make dist` — MIT OR GPL-2.0-only
.cz.toml, VERSION   Commitizen config and the version it bumps; releases run through release.yml
.github/workflows/  ci.yml (reusable-ns3-ci: strict warnings, coverage, determinism, format, headers; `cxx-standard`:
                    tools/checks/check-cxx-standard.py, core/'s standard against the image's ns-3 minimum, #141), core.yml (check `core`:
                    core/ alone at C++23, strict warnings, ctest, again under ASan + UBSan, the consumer check; #99), pr-title.yml,
                    release.yml (the org reusable-release.yml, then the ns3-swarm-core image), api-surface.yml (check
                    `api-surface`: the API record is current and nothing in it removed without a breaking mark;
                    a required check since the v1.0.0 freeze, #83)
```

## Build & verify

The step-by-step version for people (prerequisites, every command, the CI
map, troubleshooting) is [`docs/development.md`](docs/development.md).

CI is [`reusable-ns3-ci.yml`](https://github.com/swarm-routing/.github/blob/main/.github/workflows/reusable-ns3-ci.yml):
the module builds inside `ghcr.io/swarm-routing/ns3-dev:latest-{debug,sanitized}`
(warm ns-3 tree; only the module compiles) and runs its test.py suites.
`latest` is the release ns-3 supports (it maintains its latest only), so
automatic CI follows ns-3 with no edit; an older ns-3 release runs only on
demand, through a manual `workflow_dispatch` passing `ns3-versions`, a JSON list such as `["3.48"]` (org
CONTRIBUTING invariant 7). The
same locally:

```bash
docker run --rm -v "$PWD:/src/swarm-core:ro" ghcr.io/swarm-routing/ns3-dev:latest-debug \
    ns3-build-contrib --strict-warnings --test swarm-core=/src/swarm-core
```

`core/` alone, with no ns-3 tree, at C++23 as every protocol repository builds
it (plain CMake; the same strict warnings as CI, `-DSWARM_CORE_SANITIZE=ON` for
ASan + UBSan as the sanitized leg). The `core` check (`.github/workflows/core.yml`)
runs these on every PR:

```bash
cmake -S core -B build/core && cmake --build build/core && ctest --test-dir build/core
tools/checks/check-core-consumer.sh   # core/ added by another CMake project: the library alone (#100); installed + find_package, the tarball by URL (#133)
cmake --install build/core --prefix /tmp/swarm-core   # the CMake package swarm-core (find_package(swarm-core CONFIG))
```

A file added to `core/` goes into `core/swarm-core-sources.cmake` only: both
builds read it.

Microbenchmarks of the per-packet primitives and the protocols' hot paths
(`core/bench/`: a full frame encoded and decoded, `evaporate()` with ages,
the roulette draw, `DedupHistory::seen()` among them; dev-only; fetches
Google Benchmark at the commit and hash pinned in `core/CMakeLists.txt`). On a
PR, CI's `bench` job builds the base and the head on one runner, runs them
interleaved three times each and compares the median CPU time per benchmark
with `tools/bench/bench_compare.py`: the table goes to the job summary and to
one PR comment it updates in place, and a benchmark more than 15 % slower gets
a warning. Report-only, never a required check (#170):

```bash
cmake -S core -B build/bench -DCMAKE_BUILD_TYPE=Release -DSWARM_CORE_BENCH=ON
cmake --build build/bench --target swarm_core_bench && build/bench/swarm_core_bench
# the PR comparison by hand: a base build in build/base-bench, then
for i in 1 2 3; do
  build/base-bench/swarm_core_bench --benchmark_repetitions=3 --benchmark_min_time=0.2s --benchmark_out_format=json --benchmark_out=base-$i.json
  build/bench/swarm_core_bench --benchmark_repetitions=3 --benchmark_min_time=0.2s --benchmark_out_format=json --benchmark_out=head-$i.json
done
python3 -I tools/bench/bench_compare.py compare --base base-*.json --head head-*.json   # --self-test checks the script
```

Automatic runs (push, PR: `latest`) must pass `test.py -s swarm-core` on debug
and sanitized, with strict warnings as errors (our code only; ns-3's headers
are exempt), and report coverage (report-only). A manual run on older releases
(`ns3-versions`) uses its `profiles` input (default debug) without strict
warnings or coverage, since older images may predate those options. Every run
has the determinism gate: `swarm-core-seed --seed=1` twice
must be byte-identical and `--seed=2` must differ (org invariant 5). Format:
`git ls-files '*.h' '*.cc' | xargs clang-format-18 --dry-run -Werror` with the org
`.clang-format`. The suite
needs ns-3.42+ (scoped `TestCase::Duration`). **`core/` targets the C++
standard of the ns-3 release swarm-core supports**, ns-3's own
`CMAKE_CXX_STANDARD_MINIMUM` (`build-support/macros-and-definitions.cmake`:
C++17 for 3.40, C++20 for 3.41–3.45, C++23 from 3.46), so C++23 today:
`CMAKE_CXX_STANDARD 23` and `cxx_std_23` on `swarm_core` in
`core/CMakeLists.txt`, which every consumer of `swarm::core` inherits. A
consumer needs a compiler for it; when ns-3 raises its minimum, `core/`
follows in a minor release, with an issue in each consumer
(`docs/api-policy.md` "C++ standard", #141). The `cxx-standard` job of
`ci.yml` fails when `core/` is below the `ns3-dev` image's minimum
(`python3 -I tools/checks/check-cxx-standard.py --self-test`; in the image:
`python3 -I tools/checks/check-cxx-standard.py --ns3 /opt/ns-3`).
Licence headers and the include rule: `make headers` (the Go gate CI runs, through `swarm-routing/.github` `tools/checks/check-headers.sh`: docker, or go).

The public API (`docs/api-policy.md` "What the API is") is recorded in
`docs/api-surface.txt`, generated from the headers and the `TypeId`
registrations; a PR that adds to the API commits the regenerated file, and
one that removes or changes a line must be a breaking change (`!` in its
title or a `BREAKING CHANGE:` footer). The `api-surface` workflow checks both,
strict and a required check since the v1.0.0 freeze (#83). Needs `clang++-18` (the script's
`CLANG_VERSION`; `apt-get install clang-18`) and docker; the self-test runs
under any clang:

```bash
make api-surface   # = python3 -I tools/checks/check-api-surface.py generate --ns3-image $(NS3_IMAGE)
python3 -I tools/checks/check-api-surface.py --self-test
```

Releases (epic #6; `swarm-routing/.github` CONTRIBUTING "Cutting a release"):
dispatch `release.yml` with `dry-run`, then `bump`; merging the bot's
`chore(release): bump version A to B` PR tags and publishes. `make dist`
builds the asset locally (`DIST_CHECK=0` skips the docker suite and the
`core/`-alone build).

## Golden rules (invariants — do not break)

Org-wide (1–9), then this repo's own.

<!-- org-sync:begin org-rules -->
1. **Licensing per directory, as AntHocNet** ([`LICENSING.md`](LICENSING.md);
   AntHocNet [LICENSING.md](https://github.com/swarm-routing/AntHocNet/blob/main/LICENSING.md),
   [ADR-0028](https://github.com/swarm-routing/AntHocNet/blob/main/docs/adr/0028-core-is-dual-licensed.md)). A
   simulator-free `core/` and the standalone tooling are `MIT OR GPL-2.0-only`;
   ns-3 modules, the site, docs and data are `GPL-2.0-only`. Every original
   `.h`/`.hpp`/`.cc`/`.cpp`/`.go`/`.py`/`.sh` starts with
   `// Copyright (c) 2026 Daniel Joppi (https://github.com/swarm-routing)` +
   `// SPDX-License-Identifier: <MIT OR GPL-2.0-only | GPL-2.0-only>` (`#`
   comments where needed), matching its directory in `LICENSING.md`; an
   outside contributor may add their own `Copyright (c) <year> <Name>` line
   beneath the org line, never replace it, and signs off every commit (DCO,
   CONTRIBUTING "Outside contributions"). CI (`reusable-licence-check.yml`)
   checks the SPDX line, and the org line's place when a header names more
   than one holder; a header with a single, other holder (a replaced org line,
   or third-party code) passes it, so review catches that. A new directory
   gets its row in `LICENSING.md` in the same PR.
2. **The licence boundary only goes one way.** GPL code may use the dual
   parts; dual code never pulls GPL-only code in: a `core/` file includes only
   the C++ standard library, other `core/` files and the dual-licensed `core/`
   of a dependency pinned by commit and declared as the `core-deps` input of
   every workflow that runs the licence check (one calling
   `reusable-licence-check.yml`, or one calling `reusable-ns3-ci.yml`, which
   forwards it; workflow file names differ per repo), each with the same pin,
   with `core-roots` naming the repo's own `core/`
   (without it the check skips core includes; a protocol's `core/` on
   swarm-core's; CI checks it, `check-headers --core-dep`), and a dual script imports no GPL-only code. Code coming from AntHocNet keeps its
   licence: its `core/`/`tools/` (dual) may land in dual or GPL paths here; its
   `ns3/` modules and `web/` (GPL-2.0-only) only in GPL paths. Third-party code
   (AntHocNet's vendored `aomdv`/`gpsr`, IITP RAS) lands only in GPL paths with
   its provenance recorded. Write clean implementations against public ns-3
   interfaces rather than copying ns-3's `src/aodv`/`src/olsr`.
3. **Wire formats are POD + versioned.** Control headers are
   simulator-independent POD structs with bounds-checked little-endian
   readers/writers, a leading version byte, and deterministic roundtrip + fuzz
   tests. Changing a field or its units bumps the version byte.
4. **No contrib module named `core`** (collides with ns-3's `src/core`). The
   shared module is `swarm-core` (`#include "ns3/swarm-core-module.h"`), and
   every protocol builds on it, AntHocNet included: `core/` on its `core/`, the
   ns-3 adapter on `SwarmRoutingBase` (swarm-routing/swarm-adapter-ns3#14).
5. **Determinism.** A fixed `--seed=<N>` gives byte-identical output. All
   randomness through the simulator's seeded streams or an injected RNG port;
   never libc `rand()`, wall-clock time, or unordered-container iteration order
   in anything that reaches an output.
6. **Comparisons are paired.** Per-seed deltas across >= 20 seeds, emitting the
   canonical `##CONFIG##`, `##RUN##`, `##BENCH##`, `##DIAG##` lines. A
   comparator arm (a protocol we did not design) counts only after its three
   gates: a fidelity sheet from its paper, the per-PR delivery smoke, and an
   anchor reproducing the paper's own trend.
7. **Keep bounded structures bounded** (visited paths, dedup histories, queues
   carry a configured cap).
8. **CI hygiene.** Actions pinned by full commit SHA with a `# vX.Y.Z` comment;
   buildx caches scoped per profile; transient artifacts `retention-days` <= 7.
   Every job that runs steps has a `timeout-minutes` cap (a job that only
   calls a reusable workflow cannot take one: the called workflow's jobs carry it), and every apt, npm or installer step
   its own, sized from recent green runs; `apt-get update` runs with
   `--error-on=any` inside a shell retry. Ubuntu mirror outages are the usual
   cause of a red run.
9. **Nothing shipped depends on `.claude/`.** Skills hold procedures; scripts
   that anything shipped needs live in `tools/` (AntHocNet ADR-0014).
<!-- org-sync:end org-rules -->
10. **`core/` includes no simulator header.** The primitives take time, randomness, neighbours and deferred work through injected ports (as AntHocNet's `core/ports.h`), so they stay reusable, unit-testable and MIT-optional; only `model/` (GPL) includes `ns3/*`.
11. **No protocol logic here.** swarm-core holds what two or more protocols share. A protocol's decision rules live in its own repo; promote code here only when a second protocol needs it.
12. **Downstream API stability.** ARA, BeeAdHoc and later protocols build against `main`: a breaking API change lands with the downstream fixes or an issue in each consumer.
13. **Every primitive change carries a test** in `test/` (AntHocNet golden rule 7).

## Conventions

<!-- org-sync:begin conventions -->
- Issues follow the org taxonomy ([CONTRIBUTING.md](https://github.com/swarm-routing/.github/blob/main/CONTRIBUTING.md#issues), from
  [AntHocNet ADR-0013](https://github.com/swarm-routing/AntHocNet/blob/main/docs/adr/0013-track-bugs-and-findings-as-issues.md)): one type label
  (`bug`/`enhancement`/`chore`/`documentation`/`verification`, plus `epic` for
  umbrellas), area label(s) (`protocol`/`adapter`/`ns3`/`benchmark`/
  `observability`/`packaging`/`ci`/`learn-site`/`literature`), one `model:*`
  suggestion (a model on the Claude Pro plan), one `priority:P1|P2|P3` on
  non-epic issues (none on an epic), and a `release:vX.Y.Z` label only when
  a release in the repository's ladder (`docs/roadmap.md`, else the org
  ROADMAP's release train) gates the issue: that label set is the version's
  scope, released when its issues are done (CONTRIBUTING "Cutting a
  release"). Labels come from `.github`'s `labels.json`, applied to every
  repository as soon as it changes (never deleting one); a release version
  it lacks is added there (and to CONTRIBUTING's label table), or, in a
  repository that keeps its own release labels (AntHocNet), created with
  `gh label create release:vX.Y.Z`.
- Issue bodies: **Context** (evidence, numbers, links) · **Scope** ·
  **Acceptance criteria** (checkboxes) · **Depends on** (cross-repo form
  `swarm-routing/<repo>#N`) · **References**. Children of an epic say "Part of
  #N" and are linked as sub-issues. Comments you post end with the Claude Code
  footer.
- Issues migrated from AntHocNet carry `migrated` and start with *Migrated from
  `danieljoppi/AntHocNet#N`*.
- Epics are listed in a public repository's README `## Epics` and the org
  [ROADMAP](https://github.com/swarm-routing/.github/blob/main/ROADMAP.md#epics),
  generated from the `epic` issues by `.github`'s `tools/epics/epics.py`. An
  open epic is Planned (no sub-issue closed) or In progress (some closed); it
  is Done or Dropped only once the epic itself is closed (completed or not
  planned). The lists follow the issues with no hand step: an issue change
  that can move them makes the org-managed `epics-changed.yml` run org-bot's
  `epics` task, which opens the refresh PRs. Never edit a generated section
  by hand; private repositories keep none.
<!-- org-sync:end conventions -->

## Git workflow

<!-- org-sync:begin git-workflow -->
- Branch from `main` (`claude/<short-topic>`), one focused change per PR.
- **Always open a PR to `main`** once the branch is pushed — every change is
  tracked through one.
- **PRs are squash-merged** (the only merge method enabled). The PR title is
  the commit on `main`: a lowercase Conventional Commit, `type(scope)!:
  subject`, type one of `feat fix docs style refactor perf test build ci
  chore revert`, subject starting lowercase.
- Push with `git push -u origin <branch>`; merge when CI is green.
- **`main` takes no direct push**, in any repo: its ruleset requires a PR and
  the required checks, not an up-to-date branch. Bots push `bot/<name>`
  branches and open PRs; a release is the bump PR from `bot/release`, tagged
  after it merges (CONTRIBUTING "Cutting a release").
- A PR behind `main` may merge as it is. Update it
  (`gh api -X PUT "repos/<owner>/<repo>/pulls/<n>/update-branch"`, the PR's
  base repository) only when `main` changed what it touches (a template, a
  workflow, a required check), then merge after its CI. Never update a
  merge-queue PR by hand: enable auto-merge and the queue tests it on the
  latest `main` (CONTRIBUTING "Merge when CI is green"). Never force-push a
  branch someone else pushed. A behind `bot/release` PR: dispatch the release
  `bump` again.
- **Settings are code.** Settings, rulesets, required checks and the Actions
  allowlist change only by a PR to the repo's `[settings]` in
  `.github/org-sync.toml` or to `templates/rulesets/` in
  `swarm-routing/.github`, never in the UI or by a direct API write. A
  rulesets change applies on merge, a `[settings]` change on its push through
  the org-managed `settings-apply.yml` (swarm-routing/.github#569). A required
  check runs on every PR (no `paths:` filter; skip work inside a gate job) and
  is named exactly as its check run. A new third-party action goes into
  `defaults.json` `selected_actions` (`templates/rulesets/`) in swarm-routing/.github
  before or with the workflow that uses it, or `repo-settings` refuses the repo.
<!-- org-sync:end git-workflow -->

<!-- org-sync:begin skills -->
## Agent skills

The org's procedures ship as the Claude Code plugin **`swarm-routing`**
([`swarm-routing/skills`](https://github.com/swarm-routing/skills)), which this
repository's `.claude/settings.json` enables; other agents install the same
skills with `npx skills add swarm-routing/skills` ([skills.sh](https://skills.sh)).
A skill is the procedure; its script does the work without it (golden rule 9),
so an agent without the skill, or a session that does not read this
repository's settings, starts from the script. Plugin missing (it was
`org-repo` before 2026-11-09)? Run `claude plugin marketplace update
swarm-routing` and start a new session.

| Task | Script (any agent) | Skill |
|------|--------------------|-------|
| Bring this repository onto the org templates, or fix their drift | `swarm-routing/.github` `tools/org-sync/org_sync.py` | `org-repo` |
| Write or review a protocol's paper-fidelity sheet (`docs/fidelity.md`) | `swarm-routing/.github` `tools/fidelity/fidelity.py` | `fidelity-sheet` |
| Check a diff against the org golden rules before a PR or a review | `swarm-routing/.github` `tools/protocol-review/check_invariants.py` | `protocol-review` |
| Bring the README, AGENTS/CLAUDE/CONTEXT and docs/ to the org standard | `swarm-routing/.github` `tools/docs-review/check_docs.py` | `docs-review` |
| Find new papers on a topic, or confirm a reference (free sources only) | `swarm-routing/.github` `tools/lit-search/lit_search.py` | `lit-search` |
| Give a repository its docs/references.bib, or review it (every paper and org release it relies on, confirmed from its record) | `swarm-routing/.github` `tools/fidelity/fidelity.py` (bib), `tools/zenodo/zenodo.py` | `references` |
| Take open pull requests from review to merge (CI, review threads, merge) | `swarm-routing/.github` `tools/pr-status/pr_status.py` | `pr-shepherd` |
| Measure what a session loads from the agent files, or where a finished session's tokens went | `swarm-routing/skills` `tools/token-audit/token_audit.py` | `token-audit` |
| Register a repository for the OpenSSF Best Practices badge, or check its answers | `swarm-routing/.github` `cmd/bestpractices` (Go) | `bestpractices` |
| Check or bump the release a repository builds on (`swarm-core.lock`, `ns3-tools.lock`) | `swarm-routing/.github` `tools/checks/release_lock.py`, `tools/checks/check-swarm-core-pin.py` | `dependency-bump` |
| Cut a release on the maintainer's request, and its aftercare | `swarm-routing/.github` `tools/release/release_check.py`, `tools/zenodo/zenodo.py` | `release` |
| Record a reviewed SonarQube Cloud false positive or accepted finding | `swarm-routing/.github` `tools/sonar-triage/sonar_triage.py` | `sonar-triage` |

A skill only this repository uses lives in its own `.claude/skills/`; one two
repositories need moves to the plugin. Skills that file, cite and extract
papers stay in the maintainer's private library; a public skill may point to
them, never carry their content.
<!-- org-sync:end skills -->

<!-- org-sync:begin agent-defaults -->
## Working defaults

Condensed from [andrej-karpathy-skills](https://github.com/forrestchang/andrej-karpathy-skills).

- **Assumptions:** state the ones that matter. Ask only when the choice
  belongs to the maintainer (a default, a published number, a cell, the
  scope), with your recommendation and its reason, never the options alone.
  Otherwise pick the sensible option, say which, and keep going: long
  unattended runs, like campaigns, depend on that.
- **Simplicity:** the least code that solves the task. No speculative
  features, configurability or abstractions for one use.
- **Surgical diffs:** every changed line traces to the request. Match the
  surrounding style. Mention unrelated dead code rather than deleting it, and
  remove only the orphans your own change created. Update the doc or ADR of a
  documented decision you change.
- **Verifiable goals:** a bug gets a reproducing test first; a refactor passes
  before and after. Finish when the check passes, not when the code looks right.

## Session habits

- **Handoffs are always a ticket**: work in flight at a session's end, or a
  handoff asked for, is an issue from the *Handoff* template (label
  `handoff`), never a file in the repo; a session continuing a handoff
  updates that issue. Continued work starts from the newest open `handoff`
  issue for it, a new task from the request; close the issue, and any older
  one it superseded, when its work is done ([AntHocNet ADR-0013](https://github.com/swarm-routing/AntHocNet/blob/main/docs/adr/0013-track-bugs-and-findings-as-issues.md) item 5).
- **Attach only the repositories the task needs**: every request loads each
  attached repository's agent files.
- **Start from the record, not from memory:** the matching `handoff` issue,
  then the issue's acceptance criteria, then `main`. Check a claim against its
  source before it goes into an issue or a PR.
- **Script what repeats**: a check or lookup run more than a few times
  becomes a script in `tools/` that prints a summary; read the verdict, not
  the raw data (AntHocNet ADR-0014).
- **Automate what can be automated** (org CONTRIBUTING invariant 8): a step
  stays manual only when no API or permission exists for it, or the
  maintainer reserves the decision; list it in swarm-routing/.github#568.
- **Measure before cutting or adding agent text** with the `token-audit`
  skill (`swarm-routing/skills` `tools/token-audit/token_audit.py`). MCP
  servers, plugins, token habits and the long form of each pitfall below:
  [`docs/agents.md`](https://github.com/swarm-routing/.github/blob/main/docs/agents.md)
  in swarm-routing/.github (ADR-0005).

## Response style — caveman (token economy)

Answer in tight, low-filler prose: no hedging, pleasantries or preamble; same
information, fewer output tokens. **The record stays full:** code, commands,
paths, error text and benchmark numbers byte-for-byte, and commit messages,
PR bodies, ADRs, `docs/` and issue threads detailed (AntHocNet's AC_BE_NQOS /
A2 investigation was recoverable only from thorough threads). Caveman the
chat, not the record.

## Agent pitfalls

- **Before every push, run `make check`** (none: the "Build & verify"
  commands; `make pre-push TITLE='…'` where it exists) **and check the PR
  title** (Git workflow): a skipped step costs a CI round
  (swarm-routing/.github#586).
- `gh api … -F body=@file` reads the file; `-f body=@file` posts the literal
  text `@file`.
- When the permission system refuses a command (a merge, say), stop and report
  it. Never retry it another way, and never ask another agent to run it.
- Edit files with the editor tools, never a shell heredoc or `sed -i`, and
  touch one repository per command (swarm-routing/.github#578).
- Stage files by name, never `git add -A`; keep build output in `.gitignore`
  (swarm-routing/.github#644).
- A Go test reads only files under `cmd/` or `internal/` (`testdata/`)
  (swarm-routing/.github#587).
- Clean up only your own directory, never a shared one
  (swarm-routing/.github#644).
- Search an output the harness saved to a file (`grep`, `head`, `sed -n`),
  never read it whole; wait on a PR with `pr_status.py wait` or `wait
  --merged`, not by subscribing to its activity (swarm-routing/.github#794).
- A PR body's `Closes #N` closes the issue on merge; write `Part of #N` while
  acceptance criteria remain open.
- Before saying a tool or check is missing, look at `main` (`git fetch`,
  `origin/main`), not a stale local checkout.
- Write another repository's issue in full, `swarm-routing/<repo>#N`, also in
  a list or range: a bare `#N` links to the repository the text is in
  (swarm-routing/.github#277). An ns-3 item is its GitLab URL (or link text
  `nsnam/ns-3-dev#N`); text bound for ns-3's GitLab names ours by full GitHub
  URL (CONTRIBUTING "ns-3 bugs (RCAPA)").
- An issue called done in `ROADMAP.md` or an AGENTS.md is not evidence: read
  its acceptance criteria (swarm-routing/.github#274).
- Change text inside `org-sync:begin/end` markers only through
  `swarm-routing/.github` `templates/`, then org-bot's refresh PRs; never add
  such commits to an unrelated PR (swarm-routing/.github#236).
- With a merge queue, merge by enabling auto-merge (squash); a direct merge is
  refused (HTTP 405). Only the required checks (`gh api
  'repos/{owner}/{repo}/rules/branches/main'`) gate a merge; a report-only
  check (`codecov/patch`, Sonar) is triaged, not waited on. Merge an outside
  contributor's PR only after `dco` passed on its head, and never push to it
  (swarm-routing/.github#288; CONTRIBUTING "Merge when CI is green").
- `review / coderabbit` is required (swarm-routing/.github#294), so
  auto-merge may be enabled at once; still fix or answer every CodeRabbit
  thread before resolving it, and call a PR thread-free only after
  `pulls/N/comments` and the reviews show none. Where the gate is not
  required yet, wait with `pr_status.py gate REPO#N`
  (swarm-routing/.github#238).
- Never add a `Signed-off-by` trailer, for the maintainer or anyone else
  (CONTRIBUTING "Outside contributions").
- Never delete a stale branch or ask the maintainer to: org-bot's daily
  `branch-sweep` does (swarm-routing/.github#595).
- Releases are cut on request only, once the version's `release:vX.Y.Z`
  issues and its ladder exit criteria are done; never approve a gated publish;
  recover a failed publish with "Re-run all jobs"; read a DOI from Zenodo,
  never type it; never dispatch the site's `release-sync.yml` by hand
  (swarm-routing/.github#571; CONTRIBUTING "Cutting a release").
- Cite a paper by its `https://doi.org/<DOI>` link, read from its record
  (`lit-search`), or a DOI-less work by its publisher's page; never point into
  or name the private paper library.
- Never dismiss a security alert or handle a leaked secret: both are the
  maintainer's; review `security-autofix.yml`'s PRs like any other
  (swarm-routing/.github#629; CONTRIBUTING "Security alerts").
- An ns-3 bug, or its investigation, gets an RCAPA ticket in
  swarm-routing/sim-tools first (`NS3-NNN`), then its doc and register row;
  every workaround links the ID, and upstream reports go out only through
  swarm-routing/ns3-upstream-report. A regression in a published library
  version gets one in that library's repository (`<PREFIX>-NNN`), open until a
  release carries the fix; the pins and `git tag --contains` decide, never
  memory (CONTRIBUTING "RCAPA", "ns-3 bugs (RCAPA)").
<!-- org-sync:end agent-defaults -->

## Where to look

| You want to… | Go to |
|--------------|-------|
| Know which release a piece of work serves, or why something is not planned | [`docs/roadmap.md`](docs/roadmap.md) (the ladder v0.1.0 → v1.0.0, per-release exit criteria, non-goals; issues carry `release:` labels) |
| Know what the API is, whether a change is breaking, or how to deprecate something | [`docs/api-policy.md`](docs/api-policy.md) (what v1.0.0 freezes, SemVer against it, `kSwarmCoreApiVersion`, the two-step deprecation; #83) |
| Source of the primitives | AntHocNet `core/include/anthocnet/core/`, `core/src/` (MIT OR GPL-2.0-only → `core/` here); each `core/` file names its source file and commit |
| Use or extend a primitive | `core/swarm-*.h` (the API, documented in place), its test in `core/test/`, the list in `core/test/all-tests.cc` |
| Test a protocol core's trends without ns-3 (repair vs flood, bounded storms, evaporation) | `core/testbench/swarm-testbench.h` (the adapter contract and a short example in its top comment), the reference trend in `core/test/test-testbench.cc`, README "Test a protocol core without a simulator" (#15) |
| Report a protocol's internals (control by kind, route changes, data decisions, state footprint) | its core calls `swarm::core::IRouterObserver` (`core/swarm-observer.h`), the adapter passes `&Observer()` and registers its `KindName` table; read with the trace sources or `SwarmRoutingHelper::CollectStats` (#42; consumer: swarm-routing/ns3-tools#91) |
| Script node failures (shutdown, crash, restart) the same way for every arm | `SwarmLifecycleHelper` (`helper/swarm-lifecycle-helper.h`: times or a `<time s> <node> <op>` file); a protocol clears and restarts its state in `DoNodeShutdown`/`DoNodeCrash`/`DoNodeStart` on `SwarmRoutingBase`, and its core uses each primitive's `clear()` (the reset contract) (#43) |
| Write a protocol's ns-3 adapter | derive from `SwarmRoutingBase` (`model/swarm-routing-base.h`: the hooks, services and ports), install with `SwarmRoutingHelper`; the toy protocol in `test/swarm-glue-tests.cc` is a minimal example |
| Merge a change to `core/` | every protocol repository that pins swarm-core (ARA, BeeAdHoc, Termite, HOPNET) goes red on its `swarm-core / pin` check as soon as it lands, and its open PRs are blocked: open the pin-bump PR in each (`SWARM_CORE_REF` in `core/CMakeLists.txt`, the `Makefile`, every workflow's `core-deps`; the failing check names each place), verified with its core tests, until org-bot opens them (swarm-routing/.github#190, #208). A new `core/` file goes into `core/swarm-core-sources.cmake` only; the four protocol repositories link `swarm::core` (README "Use core/ from another CMake project"; #100: swarm-routing/ARA#147, swarm-routing/BeeAdHoc#125, swarm-routing/Termite#118, swarm-routing/HOPNET#121), so a new file needs no edit there beyond the pin bump |
| Source of the glue | AntHocNet `ns3/baselines/adaptive/model/adaptive-routing-base.*`, `ns3/anthocnet/model/anthocnet-{adapters,rqueue,routing-protocol}.*` (GPL-2.0-only → `model/` here) |
| Wire-format discipline | AntHocNet `docs/wire-format.md`, ADR-0006 |
| Link-metric seam history | AntHocNet #142 (closed), #145, #146 |
| The org's invariants, labels, issue shape | [`swarm-routing/.github` CONTRIBUTING.md](https://github.com/swarm-routing/.github/blob/main/CONTRIBUTING.md) |
| The reference implementation and its history | [`swarm-routing/AntHocNet`](https://github.com/swarm-routing/AntHocNet) (`AGENTS.md`, `CONTEXT.md`, `docs/adr/`) |
| Pick up open work | this repo's issues — highest open `priority:P*` first; newest open `handoff` issue for work in flight |
