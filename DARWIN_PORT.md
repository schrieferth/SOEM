# Darwin/macOS Port Notes

This branch adds an experimental Darwin/macOS backend for SOEM.

## What Was Ported

- `cmake/Darwin.cmake` selects Darwin-specific OSAL and OSHW sources.
- `osal/darwin` replaces Linux `clock_nanosleep` usage with portable
  `nanosleep`-based helpers while keeping the SOEM timer API unchanged.
- `oshw/darwin` provides adapter discovery and a BPF-based Ethernet backend.
- `slaveinfo`, `simple_ng`, `ec_sample`, `eepromtool`, `firm_update` and
  `eni_test` compile on macOS.

## Runtime Model

macOS does not provide Linux `PF_PACKET` raw sockets. The Darwin backend uses
BPF descriptors instead:

- binds a BPF descriptor to the requested interface;
- requires Ethernet DLT (`DLT_EN10MB`);
- writes complete Ethernet frames;
- reads BPF records and forwards EtherCAT (`0x88A4`) frames into the existing
  SOEM receive/index buffer logic;
- requests immediate mode, complete-link-layer writes, promiscuous mode and
  inbound-only direction when supported by the operating system.

## Verified Locally

The following commands passed on an AppleClang/macOS host:

```sh
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64
cmake --install build/darwin-arm64
build/darwin-arm64/bin/slaveinfo
build/darwin-arm64/install/bin/slaveinfo
```

Adapter discovery works and prints macOS interface names such as `en0`, `en5`
or `en6`.

A supervised hardware run on `en7` found and configured a six-slave EtherCAT
chain:

- EK1101
- EL2004
- EL1014
- EL4031
- EL3104
- EL3681

During that hardware check, `tcpdump` showed that some macOS/BPF interface
paths can expose locally transmitted EtherCAT frames before the actual slave
responses. The Darwin NIC backend therefore ignores frames whose source MAC
matches the SOEM primary or secondary source address before forwarding
EtherCAT frames into the common SOEM receive/index buffer logic. Without that
software-side self-frame filter, `slaveinfo` can observe workcounter-zero echo
traffic and report "No slaves found" even though responses are visible on the
wire.

Apple Silicon Macs from M1 through later M-series generations use the same
`arm64` architecture target from SOEM's perspective. The Darwin backend does
not use CPU-generation-specific instructions, so the same `darwin-arm64`
preset is expected to cover M1, M2, M3, M4 and M5 machines, subject to normal
macOS BPF permissions.

An Intel build for a Mac mini 2014 target can be produced with:

```sh
cmake --preset darwin-x86_64
cmake --build --preset darwin-x86_64
```

## Runtime Permission Boundary

Opening BPF for EtherCAT traffic requires root privileges on the tested macOS
host. A non-root run reaches the expected SOEM sample error:

```text
No socket connection on en0
Excecute as root
```

Run hardware inventory checks under supervised `sudo`:

```sh
sudo build/darwin-arm64/bin/slaveinfo <ethercat-interface>
```

Use the physical Ethernet interface connected to the EtherCAT segment. On the
Mac mini 2014 this will most likely be an `en*` interface reported by
`build/darwin-arm64/bin/slaveinfo` or the matching Intel build output.

## Known Limitations

- The Darwin backend is intentionally close to the Linux NIC driver, but BPF
  behavior differs from Linux raw sockets; new adapters should still be proven
  first with `slaveinfo` before moving to cyclic `simple_ng`.
- Remaining linker messages about reducing `__DATA,__common` alignment come
  from large SOEM sample buffers and did not block the build.

## Python Gateway

An experimental Python gateway is available in `python/soem_gateway/`. It uses
SOEM as external executables and does not link SOEM into Python:

```sh
python3 python/soem_gateway/soem_gateway.py --interface en6 --port 8765
```

Useful endpoints:

- `GET /health`
- `GET /status`
- `GET /adapters`
- `GET /inventory?interface=en6`
- `POST /start?interface=en6`
- `POST /stop`

Use `--inventory-with-sudo` and `--start-with-sudo` only when the gateway should
perform real EtherCAT/BPF operations that require root privileges.
