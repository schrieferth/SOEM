# HTTP API Reference

All responses are JSON objects. Operation-style responses carry `ok` (boolean),
`status` (`PASS`/`WARN`/`FAIL`) and a payload. Many endpoints have both a short
alias and an `/ethercat/...` form; they are equivalent.

Conventions:

- **Root?** = whether the underlying operation needs root/`sudo` (real EtherCAT
  traffic). Status/discovery/cached reads do not.

## Health, identity, status

| Method + path | Root? | Purpose |
| --- | --- | --- |
| `GET /health`, `GET /ethercat/health` | no | Gateway process health and resolved binary paths. |
| `GET /identity` | no | Gateway identity and license-boundary note. |
| `GET /status`, `GET /ethercat/status`, `GET /ethercat/master/status` | no | Master lifecycle state, pid, uptime, last error, log tail. |
| `GET /adapters`, `GET /ethercat/adapters` | no | Adapter list from `slaveinfo` (no interface, no traffic). |
| `GET /logs`, `GET /ethercat/logs`, `GET /ethercat/master/log?tail=<n>` | no | Master process log tail. |

## Discovery and configuration

| Method + path | Root? | Purpose |
| --- | --- | --- |
| `GET /inventory?interface=<n>`, `GET /ethercat/inventory?interface=<n>` | yes | Live `slaveinfo` inventory (slot/name/vendor/product). |
| `POST /ethercat/master/config-init` | yes | Live discovery (interface in body or `?interface=`). |
| `POST /ethercat/probe-inventory` | yes | Live discovery (interface in body). |
| `POST /ethercat/master/open` | no | Accept/validate the interface name. |
| `POST /ethercat/request-config`, `POST /ethercat/master/config-map` | no | Accept the host-generated runtime catalog. See [Runtime configuration](runtime_config.md). |
| `GET /ethercat/catalog` | no | Return the accepted runtime catalog. |

## Master lifecycle

| Method + path | Root? | Purpose |
| --- | --- | --- |
| `POST /start?interface=<n>`, `POST /ethercat/master/start`, `POST /ethercat/processdata/start` | yes | Start the `simple_ng` cyclic master. |
| `POST /ethercat/master/pause` | no | Pause the logical master. |
| `POST /stop`, `POST /ethercat/master/stop`, `POST /ethercat/processdata/stop` | no | Stop the `simple_ng` child. |

## Process data

| Method + path | Root? | Purpose |
| --- | --- | --- |
| `GET /ethercat/read?path=<path>` | no* | Read one normalized path from the cached process image. |
| `POST /ethercat/read-many` `{paths:[...]}` | no* | Read several paths from one snapshot. |
| `POST /ethercat/write` `{path,value}` | no* | Write one writable path. |
| `POST /ethercat/safe-off` | no* | Set every writable path to its safe value. |
| `GET /ethercat/snapshot`, `POST /ethercat/processdata/exchange` | no | Return the full cached process image + catalog. |

`no*`: the request itself does not require root, but values are only live while a
master started under root is running; otherwise they reflect the cached image.

## SDO mailbox (raw transport)

| Method + path | Root? | Purpose |
| --- | --- | --- |
| `GET /ethercat/slave/<slot>/sdo?index=<hex>&subindex=<hex>&value_type=<type>` | no | Raw SDO read for an address. |
| `POST /ethercat/slave/<slot>/sdo` `{index,subindex,value,value_type?}` | no | Raw SDO write for an address. |

The gateway stores/returns raw values only; names/types/access live on the host.
See [SDO](sdo.md) and [Concepts → SDO transport](../concepts/sdo_transport.md).

## Subscriptions and events

| Method + path | Root? | Purpose |
| --- | --- | --- |
| `POST /ethercat/subscriptions` `{paths,mode,deadband,min_interval_s}` | no | Create a value-change subscription. |
| `GET /ethercat/subscriptions` | no | List active subscriptions. |
| `GET /ethercat/events?subscription=<id>&since=<seq>` | no | Poll the event trace. |
| `GET /ethercat/events/stream?subscription=<id>` | no | Server-Sent-Events stream. |
| `POST /ethercat/subscriptions/<id>/cancel` | no | Cancel a subscription. |

See [Subscriptions](subscriptions.md).

## Error responses

A failed request returns `ok:false` with a `status` and `message`. Malformed input
(missing `path`, bad JSON, missing SDO `index`) returns HTTP `400`; unknown paths
return HTTP `404`; everything else is `200` with the result in the body.
