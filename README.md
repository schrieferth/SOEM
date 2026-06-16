# Simple Open EtherCAT Master Library

* Copyright (C) 2005-2025 Speciaal Machinefabriek Ketels v.o.f.
* Copyright (C) 2005-2025 Arthur Ketels
* Copyright (C) 2009-2025 RT-Labs AB, Sweden

SOEM (Simple Open EtherCAT Master) is a software library for
developing EtherCAT MainDevices.

This library is specifically designed for real-time communication in
embedded systems. Its lightweight architecture minimizes resource
consumption, making it suitable for environments with limited
resources. SOEM can also be utilized on Linux, Windows and Darwin/macOS
systems.

As a library rather than a standalone application, SOEM provides
flexibility and customization for developers looking to implement
EtherCAT technology. 

# Documentation

Upstream SOEM core documentation: https://docs.rt-labs.com/soem

This fork's additions — the Darwin/macOS port and the external-process HTTP
gateway — have their own structured documentation under [`docs/`](docs/index.md):

* [Concepts](docs/concepts/index.md) — architecture, gateway boundary, raw SDO
  transport, platform strategy.
* [Gateway](docs/gateway/index.md) — install, usage (start/stop), full HTTP API,
  runtime configuration, SDO, subscriptions, troubleshooting.
* [macOS Ports](docs/ports/index.md) — `darwin-arm64` / `darwin-x86_64` builds and
  the BPF runtime model.
* [Decision records](docs/adr/index.md).

Build it with `cd docs && make html` (Sphinx + MyST, like the Testknecht docs).

# Darwin/macOS notes

The Darwin backend uses BPF (`/dev/bpf*`) because macOS does not provide
Linux-style `PF_PACKET` raw sockets. Build and adapter discovery work as a
normal user, but opening a BPF descriptor for EtherCAT traffic normally requires
root privileges:

```sh
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64
build/darwin-arm64/bin/slaveinfo
sudo build/darwin-arm64/bin/slaveinfo en0
```

Replace `en0` with the Ethernet interface that is connected to the EtherCAT
segment. On macOS, the backend sends and receives complete Ethernet frames via
BPF, requests immediate-mode reads, requests complete-link-layer writes and
uses inbound-only BPF direction when the operating system supports it.

# Contributions

Contributions are welcome. If you want to contribute you will need to
sign a Contributor License Agreement and send it to us either by
e-mail or by physical mail. More information is available on
[https://rt-labs.com/contribution](https://rt-labs.com/contribution).
