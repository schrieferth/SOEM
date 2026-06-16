# Runtime Configuration

The gateway does **not** parse ESI XML. The automation host reads the local
catalog, matches it against the live inventory and sends only a **reduced runtime
configuration** with normalized process paths and offsets. The gateway stores it
and uses it to validate reads/writes.

## Flow

```{mermaid}
sequenceDiagram
    participant H as Automation host
    participant G as soem_gateway
    H->>G: GET /ethercat/inventory?interface=en6
    G-->>H: discovered slaves
    Note over H: host matches topology,<br/>parses local ESI,<br/>builds normalized paths
    H->>G: POST /ethercat/request-config { catalog }
    G-->>H: accepted (parameter_count)
    H->>G: POST /ethercat/master/start
    H->>G: read / write / sdo / subscribe
```

## Payload

`POST /ethercat/request-config` (alias `/ethercat/master/config-map`) accepts:

```json
{
  "interface": "en7",
  "inventory": [
    { "slot": 1, "model": "EK1101" },
    { "slot": 2, "model": "EL2004" }
  ],
  "catalog": {
    "parameters": [
      {
        "path": "ethercat.slot_2.el2004.output",
        "access": "write",
        "value_type": "bool",
        "safe_value": false,
        "process_byte_offset": 0,
        "process_bit_offset": 0
      },
      {
        "path": "ethercat.slot_3.el1014.input",
        "access": "read",
        "value_type": "bool",
        "process_byte_offset": 0,
        "process_bit_offset": 0
      }
    ]
  }
}
```

Each parameter declares:

- `path` — the normalized, vendor-neutral process path (the host owns naming).
- `access` — `read`, `write` or `read_write`.
- `value_type` — `bool`, `int`/`uint`, `float`/`real`, or other.
- `safe_value` — the value `safe-off` applies to writable paths.
- `process_byte_offset` / `process_bit_offset` — where the value sits in the flat
  process image.

## What the gateway does with it

- Stores the catalog and resets the cached process image to initial/safe values.
- Resets subscriptions and the event buffer.
- Accepts reads/writes only for paths present in the catalog; unknown paths return
  an error, and writes to read-only paths are rejected.
- Applies `safe_value` on `POST /ethercat/safe-off`.

`GET /ethercat/catalog` returns the accepted catalog;
`GET /ethercat/snapshot` returns the catalog plus the current cached values.

## Note

The cached process image makes the contract testable and usable for
moderate-rate evidence. A native helper should own deterministic real-time
process-data exchange, watchdogs and high-rate capture later, behind the same
HTTP contract. See [ADR-002](../adr/ADR-002-external-process-gateway-boundary.md).
