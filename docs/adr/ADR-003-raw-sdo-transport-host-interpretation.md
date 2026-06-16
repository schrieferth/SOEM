# ADR-003: Raw SDO Transport, Host-Side Interpretation

**Status:** accepted

## Context

CoE SDO access can be split into two responsibilities: the **transport** (perform
the mailbox read/write on the bus) and the **interpretation** (object name, data
type, read/write access, decoding to engineering values). An early iteration put
an object dictionary — names, types and access policy — into the gateway. That
duplicated interpretation that the automation host already owns for the PDO/ESI
catalog, and it placed policy in the wrong layer.

## Decision

The gateway performs only the **raw mailbox transport**; the automation host owns
the **interpretation**.

Gateway endpoints `GET`/`POST /ethercat/slave/<slot>/sdo`:

- take a resolved `(slot, index, subindex)` plus an optional `value_type`;
- store/return a raw value only;
- report the canonical hex address;
- keep no object names, types or access policy.

The host:

- maps object names to `(index, subindex)`;
- knows the type and read/write access;
- enforces read-only and rejects unknown objects before any request is sent;
- decodes raw values and produces report evidence.

Because the `simple_ng` sample does not expose CoE mailbox traffic, the current
gateway backend is a raw value store (write is remembered, unseeded read is
`null`). A native `ecx_SDO` backend can replace it without changing the contract.

## Consequences

- Read-only and unknown-object errors are host-side decisions; the gateway never
  raises them. A real transport may still return a CoE SDO abort, surfaced by the
  host as a failed result.
- Every gateway implementation — this SOEM gateway, a proxy, or a test dummy —
  exposes the identical trivial raw SDO endpoint.
- The split matches the PDO/ESI catalog ownership, so there is a single
  interpretation layer on the host.

See [Concepts → SDO transport](../concepts/sdo_transport.md) and the
[Gateway SDO page](../gateway/sdo.md).
