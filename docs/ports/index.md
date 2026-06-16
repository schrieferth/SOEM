# macOS Ports

This fork adds a Darwin/macOS backend for SOEM with two build targets — Apple
Silicon (`darwin-arm64`) and Intel (`darwin-x86_64`) — sharing one BPF-based NIC
backend.

```{toctree}
:maxdepth: 1

darwin_arm64
darwin_x86_64
bpf_runtime_model
```

## What was ported

- `cmake/Darwin.cmake` selects Darwin-specific OSAL and OSHW sources.
- `osal/darwin` replaces Linux `clock_nanosleep` usage with portable
  `nanosleep`-based helpers while keeping the SOEM timer API unchanged.
- `oshw/darwin` provides adapter discovery and a BPF-based Ethernet backend.
- The samples `slaveinfo`, `simple_ng`, `ec_sample`, `eepromtool`, `firm_update`
  and `eni_test` compile on macOS.

The two presets differ only in CPU target; the runtime model (BPF, self-frame
filter, permission boundary) is identical and documented in the
[BPF runtime model](bpf_runtime_model.md). The design rationale is in
[ADR-001](../adr/ADR-001-darwin-bpf-backend.md).
