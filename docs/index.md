# SOEM Fork Documentation — Darwin Port and HTTP Gateway

This documentation set covers the two additions this SOEM fork makes on top of the
upstream [Simple Open EtherCAT Master](https://docs.rt-labs.com/soem):

1. A **Darwin/macOS backend** (BPF-based NIC access) with `darwin-arm64` and
   `darwin-x86_64` build presets.
2. An **external-process HTTP gateway** (`python/soem_gateway/`) that exposes the
   SOEM sample executables over a small JSON API without linking SOEM into Python.

Upstream SOEM core behavior (the EtherCAT master, CoE/FoE/SoE/EoE, samples) is
documented at <https://docs.rt-labs.com/soem>. This tree documents only the
fork-specific port and gateway.

## How to read this

```{toctree}
:maxdepth: 2
:caption: Contents

concepts/index
gateway/index
ports/index
adr/index
```

## Build this documentation

The build mirrors the Testknecht documentation setup (Sphinx + MyST Markdown):

```sh
cd docs
make html        # output in docs/_build/html, warnings treated as errors
```

## Orientation

- **New here?** Read [Concepts → Architecture](concepts/architecture.md) for where
  this fork sits, then [Gateway → Install](gateway/install.md) and
  [Gateway → Usage](gateway/usage.md).
- **Running on a Mac?** See [Ports → Darwin arm64](ports/darwin_arm64.md) and the
  [BPF runtime model](ports/bpf_runtime_model.md) for permissions and the
  self-frame filter.
- **Integrating an automation system (e.g. Testknecht)?** See
  [Gateway → HTTP API](gateway/http_api.md), [Runtime configuration](gateway/runtime_config.md),
  [SDO](gateway/sdo.md) and [Subscriptions](gateway/subscriptions.md).
- **Why is it built this way?** See the [Decision records](adr/index.md).
