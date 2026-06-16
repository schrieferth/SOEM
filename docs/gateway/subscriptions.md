# Subscriptions and Events

Instead of polling each path, a client can subscribe to value changes on
configured process paths and pull a timestamped event trace.

## Create

```sh
curl -s -X POST http://127.0.0.1:8765/ethercat/subscriptions \
  -H 'Content-Type: application/json' \
  -d '{
        "paths": ["ethercat.slot_3.el1014.input"],
        "mode": "change",
        "deadband": 0,
        "min_interval_s": 0
      }'
```

- `paths` — one or more **configured** paths (must exist in the runtime catalog).
- `mode` — `change`/`edge` emits only on a real change.
- `deadband` — numeric: suppress changes smaller than this magnitude.
- `min_interval_s` — rate-limit: suppress events closer together than this.

The response carries a `subscription.id`.

## Poll or stream

```sh
# Pollable queue (optionally since a sequence number)
curl -s 'http://127.0.0.1:8765/ethercat/events?subscription=<id>&since=0'

# Server-Sent-Events stream
curl -s 'http://127.0.0.1:8765/ethercat/events/stream?subscription=<id>'
```

Each event includes the normalized path, old value, new value, timestamp,
quality, reason, subscription id and the current master status. `GET
/ethercat/subscriptions` lists active subscriptions.

## Cancel

```sh
curl -s -X POST 'http://127.0.0.1:8765/ethercat/subscriptions/<id>/cancel'
```

Subscriptions and the event buffer are also reset when a new runtime
configuration is accepted (`POST /ethercat/request-config`).

## Note

Event detection runs against the gateway's cached process image. As with process
data, a native helper should own high-rate, deterministic value-change detection
later; the client-facing contract (deadband, min-interval, sequence, quality)
stays the same.
