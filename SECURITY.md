# Security Policy

`swarm-routing/swarm-adapter-ns3` is research code for network simulation. This document records its
security posture honestly; it promises nothing the code does not do. The
org-wide policy is [swarm-routing/.github SECURITY.md](https://github.com/swarm-routing/.github/blob/main/SECURITY.md).

## Trust boundary

<!-- Which input is untrusted, the only code that parses it, and what that
code guarantees. A protocol: its wire-format decoder (org invariant 3). A
tool: the parsers of files it reads. If nothing parses untrusted input, say
so in one line. -->
The bounds-checked little-endian readers (org invariant 3) are
swarm-routing/swarm-core's (`WireReader`, `VisitedPath::read`; its
SECURITY.md), at the release `swarm-core.lock` names (#231), whose tarball the
build checks by SHA256. Here, `ReadWireBytes` (`model/swarm-wire-buffer.h`)
copies a packet's bytes out of an ns-3 `Buffer` for those readers and never
reads past the buffer's size. Everything above them (each protocol's header
layout and routing decisions) belongs to each protocol.

## Known, by-design limitations (out of scope)

- No authentication or integrity on anything: a simulation library, not a
  network stack for real radios.
- Releases are 0.x (v0.1.0 on): the API may change in any minor release;
  `kSwarmCoreApiVersion` names the API a build has. Downstream protocols
  build against `main` or pin a release.

<!-- org-sync:begin reporting -->
## Reporting a vulnerability

Report privately through GitHub's private vulnerability reporting: the
repository's **Security** tab, then **Report a vulnerability**. If that is
not available, open an issue that only asks for a private contact, and put
no vulnerability details in it. This is a research project maintained on a
best-effort basis: expect an acknowledgement within a few days, not an SLA.
<!-- org-sync:end reporting -->

## Verifying what it ships

Releases from v0.1.0 on (epic #6) are built, signed and published by the
org's release workflow (`swarm-routing/.github` `reusable-release.yml`,
called by `.github/workflows/release.yml`). Each carries
`swarm-core-X.Y.Z.tar.gz` (the module, for ns-3's `contrib/`; up to v1.0.0
with the dual-licensed `core/`, after it with `swarm-core.lock`, the
swarm-routing/swarm-core release it builds on), `SHA256SUMS`, a CycloneDX SBOM
(`<name>-X.Y.Z.cdx.json`) and signed SLSA build provenance for every
asset (`<name>-X.Y.Z.intoto.jsonl`, and the Sigstore bundle
`<name>-X.Y.Z.sigstore.json` for offline checks), where `<name>` is the name
the repository had when the release was built: `swarm-adapter-ns3` from
v2.0.0 on, `ns3-swarm-core` for v0.1.0 to v1.0.0 (renamed 2026-11-22). The workflow runs
these checks itself before it publishes, and first builds the tarball in a
clean `ns3-dev` tree and runs `test.py -s swarm-core` (up to v1.0.0 it also
built and tested `core/` alone), so a release that fails any of them never
goes out. With the
[GitHub CLI](https://cli.github.com/):

```bash
NAME=swarm-adapter-ns3   # v2.0.0 on; v0.1.0 to v1.0.0: NAME=ns3-swarm-core
sha256sum -c SHA256SUMS --ignore-missing
gh attestation verify swarm-core-X.Y.Z.tar.gz --repo "swarm-routing/$NAME" \
  --signer-workflow swarm-routing/.github/.github/workflows/reusable-release.yml
gh attestation verify swarm-core-X.Y.Z.tar.gz --repo "swarm-routing/$NAME" \
  --predicate-type https://cyclonedx.org/bom \
  --signer-workflow swarm-routing/.github/.github/workflows/reusable-release.yml
gh attestation trusted-root > trusted_root.jsonl   # once, while online
gh attestation verify swarm-core-X.Y.Z.tar.gz --repo "swarm-routing/$NAME" \
  --bundle "$NAME-X.Y.Z.sigstore.json" --custom-trusted-root trusted_root.jsonl \
  --signer-workflow swarm-routing/.github/.github/workflows/reusable-release.yml   # offline
```

The signer is the org's reusable workflow, not this repository, so every
command names it with `--signer-workflow` (without it `gh` expects a workflow
of `--repo`). `--repo` is the repository the release was built in, as its
certificate's source-repository extension records it: a release built before
the rename carries `ns3-swarm-core` and fails against `swarm-adapter-ns3`, and
the other way round. Each release after v1.0.0 also
publishes the image `ghcr.io/swarm-routing/swarm-adapter-ns3:vX.Y.Z-optimized`
and `-debug` (v0.3.0 to v1.0.0: `ghcr.io/swarm-routing/ns3-swarm-core`; v0.1.0
and v0.2.0: `ghcr.io/swarm-routing/ns3-swarm-core-ns3`; both deprecated and
frozen, deleted once the swarm-core migration completes, #255), the module
built on a pinned `ns3-dev` digest, signed with cosign
(keyless): see `swarm-routing/.github` `reusable-ns3-image.yml` for its
verify commands.
