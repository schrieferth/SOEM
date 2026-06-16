# Usage: Start, Stop, Flags

## Command line

```sh
python3 python/soem_gateway/soem_gateway.py [options]
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `--interface <name>` | none | EtherCAT NIC, e.g. `en6` (macOS) or `eth0` (Linux). Can also be passed per request. |
| `--host <addr>` | `127.0.0.1` | HTTP bind host. |
| `--port <n>` | `8765` | HTTP bind port. |
| `--pdo-server <path>` | auto-discovered | Path to `soem_pdo_server` (real cyclic process-data master). Preferred over `simple_ng` when present. |
| `--simple-ng <path>` | auto-discovered | Path to the `simple_ng` binary (legacy demo master, no per-path I/O). |
| `--slaveinfo <path>` | auto-discovered | Path to the `slaveinfo` inventory binary. |
| `--log-lines <n>` | `300` | Number of master log lines to retain. |
| `--start-with-sudo` | off | Start `simple_ng` through `sudo`. |
| `--inventory-with-sudo` | off | Run `slaveinfo` inventory through `sudo`. |

On start the gateway prints its bind address and the resolved binary paths,
including which process-data backend is active (`soem_pdo_server` or the
`cached_image` fallback).

`master/start` prefers `soem_pdo_server` (real per-path I/O) whenever its binary
is present, and only falls back to `simple_ng` (demo, no I/O) otherwise.
`/identity` and `/health` report the active `process_data` backend.

## Privilege model

Opening BPF/raw EtherCAT requires root on the tested hosts. The gateway separates
operations that need traffic from those that do not:

- **No root:** `/health`, `/status`, `/adapters`, `/ethercat/catalog`,
  `/ethercat/read` (cached), `/ethercat/snapshot`.
- **Root (sudo):** real `slaveinfo` inventory and `simple_ng` master start. Enable
  with `--inventory-with-sudo` and/or `--start-with-sudo`.

```sh
# Discovery and status only (safe, unprivileged):
python3 python/soem_gateway/soem_gateway.py --interface en6 --port 8765

# Allow real EtherCAT operations (supervised):
python3 python/soem_gateway/soem_gateway.py \
  --interface en6 --port 8765 \
  --inventory-with-sudo --start-with-sudo
```

## Lifecycle

A typical session:

```sh
# 1. Discover interfaces (no traffic)
curl -s 'http://127.0.0.1:8765/adapters'

# 2. Live inventory (needs slaveinfo + root)
curl -s 'http://127.0.0.1:8765/ethercat/inventory?interface=en6'

# 3. Send the host-generated runtime configuration
curl -s -X POST http://127.0.0.1:8765/ethercat/request-config \
  -H 'Content-Type: application/json' \
  -d @runtime_config.json

# 4. Start the master (needs simple_ng + root)
curl -s -X POST 'http://127.0.0.1:8765/ethercat/master/start?interface=en6'

# 5. Read / write process paths
curl -s 'http://127.0.0.1:8765/ethercat/read?path=ethercat.slot_3.el1014.input'
curl -s -X POST http://127.0.0.1:8765/ethercat/write \
  -H 'Content-Type: application/json' \
  -d '{"path":"ethercat.slot_2.el2004.output","value":true}'

# 6. Bring outputs safe and stop
curl -s -X POST http://127.0.0.1:8765/ethercat/safe-off
curl -s -X POST http://127.0.0.1:8765/ethercat/master/stop
```

### Stopping

- `POST /ethercat/master/stop` (alias `/stop`, `/ethercat/processdata/stop`) stops
  the `simple_ng` child.
- `Ctrl-C` / SIGINT on the gateway process stops the master and exits.
- Always prefer `POST /ethercat/safe-off` before stopping when outputs are
  energized.

See the full path list in the [HTTP API reference](http_api.md).
