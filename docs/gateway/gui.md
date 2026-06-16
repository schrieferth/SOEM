# Control Panel (GUI)

`python/soem_gateway/gateway_gui.py` is a small desktop control panel that shows
the EtherCAT master status and lets an operator set the master state. It uses
only the Python standard library — **Tkinter** (the GUI shipped with Python, no
Qt) and `urllib` — and is a thin HTTP client for a running gateway.

## Run

Start a gateway first (see [Usage](usage.md)), then:

```sh
python3 python/soem_gateway/gateway_gui.py --url http://127.0.0.1:8765 --interface en6
```

| Flag | Default | Meaning |
| --- | --- | --- |
| `--url <base>` | `http://127.0.0.1:8765` | Gateway base URL (also editable in the window). |
| `--interface <name>` | empty | Default interface used by Start and Inventory. |

The gateway URL and interface can also be changed in the top bar at runtime.

## What it shows

Polled from `GET /ethercat/master/status` (every 2 s while **Auto-refresh** is on):

- a large, colour-coded **state** — `RUNNING` (green), `STOPPED` (grey),
  `FAILED` (red), `PAUSED` (amber), or `DISCONNECTED` if the gateway is
  unreachable;
- interface, PID, uptime, last return code and last error;
- **Slaves** count (filled by the Inventory button);
- an activity/log area showing actions, gateway responses and the master log
  tail.

## What it sets

The **Set master state** buttons map directly to the gateway endpoints:

| Button | Endpoint |
| --- | --- |
| Start | `POST /ethercat/master/start?interface=<iface>` |
| Pause | `POST /ethercat/master/pause` |
| Stop | `POST /ethercat/master/stop` |
| Safe-off | `POST /ethercat/safe-off` |
| Inventory | `GET /ethercat/inventory?interface=<iface>` |

After each action the panel refreshes the status automatically.

## Notes

- All HTTP calls run on background threads, so a slow or unreachable gateway
  never freezes the window; an unreachable gateway simply shows `DISCONNECTED`.
- Real Start/Inventory need a built master binary, a connected segment and root
  on the gateway side (see [Usage → Privilege model](usage.md#privilege-model)).
  `Pause` is accepted by the API but is a no-op in the current CLI backend.
- The GUI is a pure client: it adds no interpretation and holds no state beyond
  what it displays. It works against this SOEM gateway or any service that
  exposes the same endpoints.

## Headless / tests

The GUI's HTTP client is covered by `test_gateway_gui.py`, which runs without a
display (it never opens a window):

```sh
python3 python/soem_gateway/test_gateway_gui.py
```

On a headless host, launching the window prints `Tkinter is not available` and
exits non-zero instead of crashing.
