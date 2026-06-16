# Gateway Boundary

The gateway is an integration layer, not a SOEM binding. This page explains the
boundary it keeps and why.

## SOEM stays an external executable

`soem_gateway.py` supervises SOEM sample programs as separate processes:

- `slaveinfo <interface>` — one-shot adapter/inventory probe; stdout is parsed.
- `simple_ng <interface>` — cyclic master process; started/stopped as a child.

It never imports SOEM, never links `libsoem`, and never embeds SOEM source. The
only coupling is the process boundary (argv + standard streams). This keeps the
SOEM license boundary explicit: a system that must not link SOEM into its own
runtime can still drive EtherCAT through this gateway.

## The gateway is deliberately dumb

The gateway holds only mechanical state:

- the **runtime configuration** the host generated (normalized paths, process
  byte/bit offsets) — see [Runtime configuration](../gateway/runtime_config.md);
- a **cached process image** of the most recent read/written values;
- **subscriptions** and a bounded event buffer;
- a **raw SDO value store** (no object names/types/access).

It does **not** parse ESI XML, does not know object names or engineering units,
and does not enforce read-only policy. All of that interpretation lives on the
automation host. This mirrors the Testknecht EtherCAT gateway contract so the two
gateways present the same minimal surface.

## Consequences

- The host can point at this gateway directly, or proxy through another service
  (e.g. a Signalzwerg) with no behavioral difference, because the gateway adds no
  interpretation of its own.
- Read-only rejection and unknown-object errors happen on the host before a
  request is sent; the gateway only performs raw transport.
- A future native `ecx_SDO`/process-data backend can replace the cached image and
  raw SDO store without changing the HTTP contract.

See [ADR-002](../adr/ADR-002-external-process-gateway-boundary.md) and
[ADR-003](../adr/ADR-003-raw-sdo-transport-host-interpretation.md).
