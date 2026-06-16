# ADR-002: External-Process HTTP Gateway Boundary

**Status:** accepted

## Context

Automation systems (such as Testknecht) need to drive EtherCAT from a host that
must not link SOEM into its own runtime — both for a clean license boundary and
so the real-time-adjacent master code stays in a separate process. A stable,
language-neutral integration surface is needed.

## Decision

Provide `python/soem_gateway/soem_gateway.py`: a small HTTP/JSON server that
**supervises SOEM sample binaries as external processes** and never imports or
links SOEM.

- `slaveinfo <interface>` is run for one-shot adapter/inventory probes; its
  stdout is parsed.
- `simple_ng <interface>` is started/stopped as a child process for the cyclic
  master, and a process image is cached in the gateway.
- The HTTP contract is the product surface, not a Python import. The API
  deliberately mirrors the shape of pySOEM-style wrappers (master lifecycle,
  config-init/config-map, process-data read/write, SDO, safe-off, logs,
  subscriptions) so it is familiar, but it does not depend on pySOEM.

The server is unprivileged by default. Only operations that touch real EtherCAT
traffic need root, gated by `--inventory-with-sudo` / `--start-with-sudo`.

## Consequences

- SOEM stays an external executable boundary; consumers integrate over HTTP.
- The gateway can run on Linux or macOS with the matching native binaries; the
  HTTP contract is identical (see [Platform strategy](../concepts/platform_strategy.md)).
- The same surface can be re-exposed by a proxy (for example a Signalzwerg) with
  no behavioral change, because the gateway adds no interpretation.
- The current backend caches a process image and is not yet a deterministic
  real-time process-data path; a native helper can replace `simple_ng`-based
  cycling later behind the same HTTP contract.

See [Concepts → Gateway boundary](../concepts/gateway_boundary.md) and the
[HTTP API reference](../gateway/http_api.md).
