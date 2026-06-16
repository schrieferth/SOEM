# Architecture

This fork has two layers on top of the upstream SOEM master: a platform NIC
backend (Darwin/BPF) and an external-process HTTP gateway. An automation system
such as Testknecht never links SOEM; it speaks JSON to the gateway, and the
gateway supervises the SOEM sample binaries.

## Layers

```{mermaid}
flowchart TD
    A["Automation host (e.g. Testknecht)\nowns ESI/catalog interpretation,\nSDO object catalog, safety, evidence"]
    B["soem_gateway.py\nHTTP/JSON server\nsupervises external SOEM binaries"]
    C["SOEM sample binaries\nslaveinfo, simple_ng"]
    D["SOEM master core\n(CoE/FoE/SoE/EoE, process data)"]
    E["NIC backend\nLinux raw socket / Darwin BPF"]
    F["EtherCAT segment\ncoupler + terminals"]
    A -- "HTTP/JSON" --> B
    B -- "subprocess (stdout/stdin)" --> C
    C --> D
    D --> E
    E --> F
```

## Who owns what

| Concern | Owner |
| --- | --- |
| ESI/catalog interpretation, normalized paths, SDO object catalog (names/types/access), safety policy, report evidence | Automation host |
| HTTP/JSON contract, process control, cached process image, subscriptions, raw mailbox transport | `soem_gateway.py` |
| EtherCAT master protocol, cyclic process data, CoE mailbox | SOEM core + sample binaries |
| Raw frame I/O on the NIC | Platform backend (Linux raw sockets / Darwin BPF) |

The guiding rule is the same one Testknecht uses for its EtherCAT gateway
contract: **interpretation lives on the host, transport lives at the gateway.**
The gateway is kept dumb so the same trivial endpoints work whether the host
talks to this SOEM gateway directly or through a Signalzwerg proxy. See
[Gateway boundary](gateway_boundary.md) and [SDO transport](sdo_transport.md).

## Process boundary

The gateway does **not** import or link SOEM. It starts `slaveinfo` (one-shot
inventory) and `simple_ng` (cyclic master) as separate executables and
communicates over their standard streams. This keeps the GPL/commercial SOEM
boundary explicit and lets the gateway run unprivileged for status/discovery
while only the real EtherCAT operations need elevated rights. See
[ADR-002](../adr/ADR-002-external-process-gateway-boundary.md).

## Platform portability

The same gateway code runs on Linux (raw-socket SOEM build) and macOS
(`darwin-arm64`/`darwin-x86_64` BPF build). Only the native SOEM binaries differ;
`soem_gateway.py` locates per-platform build outputs automatically. See
[Platform strategy](platform_strategy.md) and [Ports](../ports/index.md).
