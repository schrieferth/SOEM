# ADR-001: Darwin/macOS NIC Backend via BPF

**Status:** accepted

## Context

Upstream SOEM reaches the network through a platform OSHW backend. The Linux
backend uses `PF_PACKET` raw sockets, which macOS does not provide. To run the
SOEM samples on macOS (developer machines, a Mac-based bench) a Darwin backend is
needed without changing the SOEM master core or its timer API.

## Decision

Add a Darwin/macOS backend selected by `cmake/Darwin.cmake`:

- `osal/darwin` replaces Linux `clock_nanosleep` usage with portable
  `nanosleep`-based helpers while keeping the SOEM timer API unchanged.
- `oshw/darwin` provides adapter discovery and a **BPF-based** Ethernet backend:
  it binds a BPF descriptor to the requested interface, requires Ethernet DLT
  (`DLT_EN10MB`), writes complete Ethernet frames, reads BPF records and forwards
  EtherCAT (`0x88A4`) frames into the existing SOEM receive/index buffer logic.
  It requests immediate mode, complete-link-layer writes, promiscuous mode and
  inbound-only direction where the OS supports them.
- The samples `slaveinfo`, `simple_ng`, `ec_sample`, `eepromtool`, `firm_update`
  and `eni_test` compile on macOS.

Two build presets are provided: `darwin-arm64` (Apple Silicon) and
`darwin-x86_64` (Intel).

## Self-frame filter

During a supervised hardware run, `tcpdump` showed that some macOS/BPF interface
paths expose locally transmitted EtherCAT frames before the slave responses.
The Darwin backend therefore **ignores frames whose source MAC matches the SOEM
primary or secondary source address** before forwarding into the common receive
logic. Without this software self-frame filter, `slaveinfo` can observe
workcounter-zero echo traffic and wrongly report "No slaves found".

## Consequences

- macOS becomes a supported platform for the SOEM samples and the gateway.
- Opening BPF for EtherCAT requires root; status/discovery without traffic does
  not. See the [Runtime permission boundary](../ports/bpf_runtime_model.md).
- BPF behavior differs from Linux raw sockets; a new adapter should be proven
  with `slaveinfo` before cyclic `simple_ng`.
- The master core and timer API are unchanged, so upstream behavior is preserved
  on Linux.

A six-slave chain (EK1101, EL2004, EL1014, EL4031, EL3104, EL3681) was found and
configured on `en7` on an Apple Silicon host. See
[Ports → BPF runtime model](../ports/bpf_runtime_model.md).
