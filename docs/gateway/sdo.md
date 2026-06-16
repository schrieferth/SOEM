# SDO Mailbox Access

CoE SDO access reaches the object dictionary by a 16-bit index and 8-bit
subindex, separate from the cyclic process image. The gateway provides only the
**raw transport**; object names, types, access policy and decoding live on the
automation host. See [Concepts → SDO transport](../concepts/sdo_transport.md) and
[ADR-003](../adr/ADR-003-raw-sdo-transport-host-interpretation.md).

## Endpoints

```text
GET  /ethercat/slave/<slot>/sdo?index=<hex>&subindex=<hex>&value_type=<type>
POST /ethercat/slave/<slot>/sdo   { "index", "subindex", "value", "value_type"? }
```

- The slave is addressed by `slot` in the path.
- `index`/`subindex` accept ints, `0x..` or ESI-style `#x..`; the response always
  reports canonical lowercase hex (`0x1018` / `0x01`).
- `value_type` is an optional transport hint (size/type).

## Examples

```sh
# Raw read of identity object 0x1018:1 (vendor id), typed as uint
curl -s 'http://127.0.0.1:8765/ethercat/slave/2/sdo?index=0x1018&subindex=0x01&value_type=uint'
# -> { "ok": true, "value": 2, "index": "0x1018", "subindex": "0x01" }

# Raw write of a commissioning object
curl -s -X POST http://127.0.0.1:8765/ethercat/slave/2/sdo \
  -H 'Content-Type: application/json' \
  -d '{"index":"0x8000","subindex":"0x01","value":3,"value_type":"int"}'
# -> { "ok": true, "value": 3, "index": "0x8000", "subindex": "0x01" }
```

## Current backend behavior

The `simple_ng` sample does not expose CoE mailbox traffic, so the current
backend is a **raw value store**:

- a write is remembered and read back;
- an object that was never written reads back as `null`;
- there is no object dictionary, no read-only enforcement and no name lookup —
  those are host responsibilities.

A native `ecx_SDO` backend can replace the raw store later without changing this
contract.

## What the host adds

When an automation host (such as Testknecht) uses these endpoints, it resolves
the object name to `(index, subindex)`, supplies `value_type`, enforces read-only
on its side, decodes the raw value and records evidence. The gateway stays
unaware of all of that.
