#!/usr/bin/env python3
"""Tkinter control panel for the SOEM gateway.

A small desktop UI that shows the EtherCAT master status and lets an operator set
the master state (start / pause / stop / safe-off). It is a thin HTTP client for
``soem_gateway.py`` and uses only the Python standard library — Tkinter for the
GUI and ``urllib`` for the requests — so it needs no extra dependencies and no
Qt.

Run it next to a running gateway:

    python3 python/soem_gateway/gateway_gui.py --url http://127.0.0.1:8765 --interface en6

The panel polls ``GET /ethercat/master/status`` and drives the master through the
``POST /ethercat/master/{start,pause,stop}`` and ``POST /ethercat/safe-off``
endpoints. HTTP calls run on background threads so a slow or unreachable gateway
never freezes the window.
"""

from __future__ import annotations

import argparse
import json
import threading
from typing import Any, Callable
from urllib import error, parse, request


DEFAULT_URL = "http://127.0.0.1:8765"
POLL_INTERVAL_MS = 2000


class GatewayClient:
    """Standard-library HTTP client for the SOEM gateway control endpoints.

    Every method returns a JSON dict. Transport failures become a structured
    ``{"ok": False, "error": "gateway_unavailable", ...}`` result instead of an
    exception, so the GUI can show "disconnected" without special-casing.
    """

    def __init__(self, base_url: str = DEFAULT_URL, timeout_s: float = 5.0) -> None:
        self.base_url = base_url.rstrip("/")
        self.timeout_s = float(timeout_s)

    def identity(self) -> dict[str, Any]:
        return self._request("GET", "/identity")

    def master_status(self) -> dict[str, Any]:
        return self._request("GET", "/ethercat/master/status")

    def master_start(self, interface: str | None) -> dict[str, Any]:
        suffix = f"?interface={parse.quote(interface, safe='')}" if interface else ""
        return self._request("POST", f"/ethercat/master/start{suffix}", {})

    def master_pause(self) -> dict[str, Any]:
        return self._request("POST", "/ethercat/master/pause", {})

    def master_stop(self) -> dict[str, Any]:
        return self._request("POST", "/ethercat/master/stop", {})

    def safe_off(self) -> dict[str, Any]:
        return self._request("POST", "/ethercat/safe-off", {})

    def inventory(self, interface: str | None) -> dict[str, Any]:
        suffix = f"?interface={parse.quote(interface, safe='')}" if interface else ""
        return self._request("GET", f"/ethercat/inventory{suffix}")

    def _request(self, method: str, path: str, payload: dict[str, Any] | None = None) -> dict[str, Any]:
        url = f"{self.base_url}{path}"
        data = None if payload is None else json.dumps(payload).encode("utf-8")
        req = request.Request(url, data=data, method=method, headers={"Content-Type": "application/json"})
        try:
            with request.urlopen(req, timeout=self.timeout_s) as response:
                return _parse_json(response.read().decode("utf-8"))
        except error.HTTPError as exc:
            raw = exc.read().decode("utf-8") if exc.fp is not None else ""
            return _parse_json(raw) or {
                "ok": False,
                "error": "gateway_http_error",
                "message": f"Gateway returned {exc.code} {exc.reason}.",
            }
        except (error.URLError, OSError) as exc:
            return {
                "ok": False,
                "error": "gateway_unavailable",
                "message": f"Gateway {self.base_url} is unavailable: {exc}.",
            }


def _parse_json(raw: str) -> dict[str, Any]:
    if not raw:
        return {}
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        return {"ok": False, "error": "invalid_response", "message": raw[:200]}
    return data if isinstance(data, dict) else {"ok": False, "error": "invalid_response"}


# State string -> display colour. Unknown states fall back to a neutral colour.
_STATE_COLORS = {
    "RUNNING": "#1a7f37",
    "STOPPED": "#57606a",
    "FAILED": "#cf222e",
    "PAUSED": "#9a6700",
}


def run_gui(base_url: str = DEFAULT_URL, interface: str = "") -> int:
    """Open the control panel. Returns 1 if Tkinter is unavailable (headless)."""

    try:
        import tkinter as tk
        from tkinter import scrolledtext, ttk
    except Exception as exc:  # pragma: no cover - depends on the OS Tk build.
        print(f"Tkinter is not available: {exc}")
        return 1

    client = GatewayClient(base_url)

    root = tk.Tk()
    root.title("SOEM Gateway — Master Control")
    root.minsize(560, 480)

    url_var = tk.StringVar(value=base_url)
    iface_var = tk.StringVar(value=interface)
    auto_var = tk.BooleanVar(value=True)

    # -- connection bar ------------------------------------------------------
    bar = ttk.Frame(root, padding=8)
    bar.pack(fill="x")
    ttk.Label(bar, text="Gateway URL:").pack(side="left")
    ttk.Entry(bar, textvariable=url_var, width=28).pack(side="left", padx=(4, 12))
    ttk.Label(bar, text="Interface:").pack(side="left")
    ttk.Entry(bar, textvariable=iface_var, width=10).pack(side="left", padx=(4, 12))
    ttk.Checkbutton(bar, text="Auto-refresh", variable=auto_var).pack(side="left")

    # -- status panel --------------------------------------------------------
    status = ttk.LabelFrame(root, text="Master status", padding=8)
    status.pack(fill="x", padx=8, pady=(0, 8))

    state_var = tk.StringVar(value="—")
    state_label = tk.Label(status, textvariable=state_var, font=("TkDefaultFont", 18, "bold"))
    state_label.grid(row=0, column=0, columnspan=4, sticky="w", pady=(0, 6))

    fields: dict[str, tk.StringVar] = {}
    rows = [
        ("Interface", "interface"),
        ("PID", "pid"),
        ("Uptime (s)", "uptime_s"),
        ("Slaves", "slaves"),
        ("Return code", "last_returncode"),
        ("Last error", "last_error"),
    ]
    for index, (label_text, key) in enumerate(rows):
        var = tk.StringVar(value="—")
        fields[key] = var
        col = (index % 2) * 2
        line = 1 + index // 2
        ttk.Label(status, text=f"{label_text}:").grid(row=line, column=col, sticky="e", padx=(0, 4), pady=2)
        ttk.Label(status, textvariable=var).grid(row=line, column=col + 1, sticky="w", padx=(0, 16), pady=2)

    # -- controls ------------------------------------------------------------
    controls = ttk.LabelFrame(root, text="Set master state", padding=8)
    controls.pack(fill="x", padx=8, pady=(0, 8))

    # -- log -----------------------------------------------------------------
    logframe = ttk.LabelFrame(root, text="Activity / master log", padding=8)
    logframe.pack(fill="both", expand=True, padx=8, pady=(0, 8))
    logbox = scrolledtext.ScrolledText(logframe, height=12, state="disabled", wrap="word")
    logbox.pack(fill="both", expand=True)

    def log(message: str) -> None:
        logbox.configure(state="normal")
        logbox.insert("end", message.rstrip() + "\n")
        logbox.see("end")
        logbox.configure(state="disabled")

    def call_async(fn: Callable[[], dict[str, Any]], on_done: Callable[[dict[str, Any]], None]) -> None:
        """Run an HTTP call off the UI thread, then apply the result on it."""

        client.base_url = url_var.get().rstrip("/")

        def worker() -> None:
            result = fn()
            root.after(0, lambda: on_done(result))

        threading.Thread(target=worker, daemon=True).start()

    def show_status(data: dict[str, Any]) -> None:
        if data.get("error"):
            state_var.set("DISCONNECTED")
            state_label.configure(fg="#cf222e")
            for var in fields.values():
                var.set("—")
            return
        state = str(data.get("master_status") or ("RUNNING" if data.get("running") else "STOPPED"))
        state_var.set(state)
        state_label.configure(fg=_STATE_COLORS.get(state, "#9a6700"))
        fields["interface"].set(str(data.get("interface") or "—"))
        fields["pid"].set(str(data.get("pid") if data.get("pid") is not None else "—"))
        fields["uptime_s"].set(str(data.get("uptime_s") if data.get("uptime_s") is not None else "—"))
        fields["last_returncode"].set(str(data.get("last_returncode") if data.get("last_returncode") is not None else "—"))
        fields["last_error"].set(str(data.get("last_error") or "—"))

    def show_status_with_log(data: dict[str, Any]) -> None:
        show_status(data)
        for line in data.get("log_tail", [])[-5:] if isinstance(data.get("log_tail"), list) else []:
            log(f"[master] {line}")

    def on_action(label: str, data: dict[str, Any]) -> None:
        ok = "ok" if data.get("ok") else "FAILED"
        message = data.get("message") or data.get("error") or ""
        log(f"{label}: {ok} {('— ' + str(message)) if message else ''}".rstrip())
        # Always refresh status after an action so the panel reflects the change.
        call_async(client.master_status, show_status)

    def on_inventory(data: dict[str, Any]) -> None:
        slaves = data.get("slaves")
        count = len(slaves) if isinstance(slaves, list) else "—"
        fields["slaves"].set(str(count))
        if data.get("error"):
            log(f"Inventory: FAILED — {data.get('message')}")
        else:
            log(f"Inventory: {count} slave(s) on {iface_var.get() or 'default interface'}")

    # control buttons
    ttk.Button(controls, text="Start", command=lambda: (
        log("Start requested…"),
        call_async(lambda: client.master_start(iface_var.get() or None), lambda d: on_action("Start", d)),
    )).pack(side="left", padx=4)
    ttk.Button(controls, text="Pause", command=lambda: (
        log("Pause requested…"),
        call_async(client.master_pause, lambda d: on_action("Pause", d)),
    )).pack(side="left", padx=4)
    ttk.Button(controls, text="Stop", command=lambda: (
        log("Stop requested…"),
        call_async(client.master_stop, lambda d: on_action("Stop", d)),
    )).pack(side="left", padx=4)
    ttk.Button(controls, text="Safe-off", command=lambda: (
        log("Safe-off requested…"),
        call_async(client.safe_off, lambda d: on_action("Safe-off", d)),
    )).pack(side="left", padx=4)
    ttk.Button(controls, text="Inventory", command=lambda: (
        call_async(lambda: client.inventory(iface_var.get() or None), on_inventory),
    )).pack(side="left", padx=12)
    ttk.Button(bar, text="Refresh", command=lambda: call_async(client.master_status, show_status_with_log)).pack(side="left", padx=(12, 0))

    def poll() -> None:
        if auto_var.get():
            call_async(client.master_status, show_status)
        root.after(POLL_INTERVAL_MS, poll)

    log(f"Connecting to {base_url} …")
    call_async(client.master_status, show_status_with_log)
    poll()
    root.mainloop()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Tkinter control panel for the SOEM gateway.")
    parser.add_argument("--url", default=DEFAULT_URL, help="Gateway base URL.")
    parser.add_argument("--interface", default="", help="Default EtherCAT interface for start/inventory.")
    args = parser.parse_args(argv)
    return run_gui(args.url, args.interface)


if __name__ == "__main__":
    raise SystemExit(main())
