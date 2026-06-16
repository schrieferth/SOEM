# SOEM HTTP Gateway

`python/soem_gateway/soem_gateway.py` exposes the SOEM sample executables over a
small HTTP/JSON API without linking SOEM into Python. It is the integration
surface for automation systems such as Testknecht.

```{toctree}
:maxdepth: 1

install
usage
gui
http_api
runtime_config
sdo
subscriptions
troubleshooting
```

## At a glance

- **Start:** `python3 python/soem_gateway/soem_gateway.py --interface en6 --port 8765`
- **No root needed for:** `/health`, `/status`, `/adapters`, catalog/read of cached
  values.
- **Root (sudo) needed for:** real inventory and master start — gated by
  `--inventory-with-sudo` / `--start-with-sudo`.
- **Boundary:** SOEM runs as external `slaveinfo` / `simple_ng` processes; nothing
  is linked into Python. See [Concepts → Gateway boundary](../concepts/gateway_boundary.md).
- **Control panel:** a stdlib Tkinter UI shows and sets the master state — see
  [Control Panel (GUI)](gui.md).
