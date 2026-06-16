# BPF Runtime Model

macOS does not provide Linux `PF_PACKET` raw sockets, so the Darwin backend uses
BPF descriptors. This page describes the runtime model, the permission boundary
and the self-frame filter that the arm64 and x86_64 builds share.

## How the backend uses BPF

The Darwin NIC backend:

- binds a BPF descriptor to the requested interface;
- requires Ethernet DLT (`DLT_EN10MB`);
- writes complete Ethernet frames;
- reads BPF records and forwards EtherCAT (`0x88A4`) frames into the existing
  SOEM receive/index buffer logic;
- requests immediate mode, complete-link-layer writes, promiscuous mode and
  inbound-only direction when the operating system supports them.

The SOEM master core and its timer API are unchanged; only the link-layer I/O is
Darwin-specific.

## Self-frame filter

During a supervised hardware run, `tcpdump` showed that some macOS/BPF interface
paths expose **locally transmitted** EtherCAT frames before the actual slave
responses. The Darwin backend therefore ignores frames whose source MAC matches
the SOEM primary or secondary source address before forwarding EtherCAT frames
into the common receive/index buffer logic.

Without this software-side self-frame filter, `slaveinfo` can observe
workcounter-zero echo traffic and report **"No slaves found"** even though
responses are visible on the wire.

## Permission boundary

Opening BPF for EtherCAT traffic requires root on the tested macOS hosts. A
non-root run reaches the expected SOEM sample error:

```text
No socket connection on en0
Excecute as root
```

Run hardware inventory checks under supervised `sudo`:

```sh
sudo build/darwin-arm64/bin/slaveinfo <ethercat-interface>
```

Status and adapter discovery without traffic do **not** need root. The HTTP
gateway uses this split: it can run unprivileged for `/health`, `/status` and
`/adapters`, and only escalates for real inventory/master operations via
`--inventory-with-sudo` / `--start-with-sudo` (see [Gateway → Usage](../gateway/usage.md)).

## Verified locally

The following passed on an AppleClang/macOS host:

```sh
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64
cmake --install build/darwin-arm64
build/darwin-arm64/bin/slaveinfo
build/darwin-arm64/install/bin/slaveinfo
```

Adapter discovery printed macOS interface names such as `en0`, `en5` and `en6`.
A supervised run on `en7` found and configured a six-slave chain (EK1101, EL2004,
EL1014, EL4031, EL3104, EL3681).

## Known limitations

- The Darwin backend is intentionally close to the Linux NIC driver, but BPF
  behavior differs from Linux raw sockets; prove a new adapter with `slaveinfo`
  before cyclic `simple_ng`.
- Remaining linker messages about reducing `__DATA,__common` alignment come from
  large SOEM sample buffers and did not block the build.
