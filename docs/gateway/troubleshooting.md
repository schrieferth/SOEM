# Troubleshooting

## `No socket connection on <iface>` / `Excecute as root`

Opening BPF/raw EtherCAT needs root. Either run inventory/start with the sudo
flags, or run the gateway with sufficient privileges:

```sh
python3 python/soem_gateway/soem_gateway.py --interface en6 --port 8765 \
  --inventory-with-sudo --start-with-sudo
```

`/health`, `/status` and `/adapters` work without root; only real traffic does
not.

## `slaveinfo` reports "No slaves found" but traffic is visible

On some macOS/BPF interface paths, locally transmitted EtherCAT frames are echoed
back before the slave responses. The Darwin backend filters frames whose source
MAC matches the SOEM source address; if you see workcounter-zero echo traffic,
confirm you are on a build that includes the self-frame filter and that you are
using the physical Ethernet interface connected to the segment. See the
[BPF runtime model](../ports/bpf_runtime_model.md).

## Binary not found

If inventory/start report a missing binary, build the samples for your platform
(see [Install](install.md)) or pass explicit paths:

```sh
--slaveinfo /path/to/slaveinfo --simple-ng /path/to/simple_ng
```

The gateway's auto-discovery search order is listed in
[Install → Binary discovery](install.md#binary-discovery).

## Wrong interface

Use the physical Ethernet interface connected to the EtherCAT segment. List
candidates without traffic:

```sh
curl -s http://127.0.0.1:8765/adapters
```

On macOS this is typically an `en*` interface; on the tested bench it was `en7`.

## Reads return stale or cached values

Process-data reads reflect the gateway's cached image. Values are only live while
a master started under root is running. Start the master first
(`POST /ethercat/master/start`) and confirm `GET /ethercat/master/status` shows
`RUNNING`.

## Unknown path / read-only path

The gateway accepts process reads/writes only for paths in the runtime catalog.
If you get `unknown` or `read-only` errors, re-send the runtime configuration
(`POST /ethercat/request-config`) and check the `access` of the parameter. For
SDO, object names/types/access are a host responsibility — see [SDO](sdo.md).
