# SDO: Raw Transport vs Host Interpretation

CoE SDO (Service Data Object) access reaches the mailbox object dictionary by a
16-bit index and an 8-bit subindex. It is separate from the cyclic process image.
This fork follows a strict split: **the gateway performs only the raw mailbox
transport; the automation host owns the interpretation.**

## What the gateway does

`GET /ethercat/slave/<slot>/sdo?index=<hex>&subindex=<hex>&value_type=<type>` and
`POST /ethercat/slave/<slot>/sdo` operate on a raw value for a `(slot, index,
subindex)` address:

- index/subindex accept ints, `0x..` or ESI-style `#x..` and are reported in
  canonical lowercase hex (`0x1018` / `0x01`);
- the optional `value_type` tells the transport the size/type;
- the gateway returns/stores the raw value only.

Because the `simple_ng` sample does not expose CoE mailbox traffic yet, the
current backend is a raw store: a write is remembered and read back, and an
object that was never written reads back as `null`. A future native `ecx_SDO`
backend can replace this store without changing the wire contract.

```text
GET /ethercat/slave/2/sdo?index=0x1018&subindex=0x01&value_type=uint
{ "ok": true, "value": 2, "index": "0x1018", "subindex": "0x01" }
```

## What the host does

The automation host owns the SDO object catalog:

- maps object **names** to `(index, subindex)`;
- knows the **data type** and **read/write access**;
- enforces **read-only** and rejects **unknown objects** before any request is
  sent to the gateway;
- **decodes** the raw value into an engineering value and builds report evidence.

So errors such as "read-only object" or "unknown object" are host-side decisions;
the gateway never sees them. A backend may still fail a transport (for example a
real CoE SDO abort for a non-existent object on the slave), which the host
surfaces as a failed result.

## Why this split

Keeping names, types and access on the host means:

- one place owns interpretation (the same place that owns the ESI/PDO catalog);
- every gateway implementation — this SOEM gateway, a proxy, or a test dummy —
  exposes the identical trivial raw endpoint;
- the gateway stays replaceable and dumb.

See [ADR-003](../adr/ADR-003-raw-sdo-transport-host-interpretation.md).
