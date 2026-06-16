#!/usr/bin/env python3
"""Tests for the gateway GUI's HTTP client.

These exercise ``GatewayClient`` against the real gateway HTTP handler. They do
not open a Tkinter window, so they run headless (CI, no display).
"""

from __future__ import annotations

from http.server import ThreadingHTTPServer
from pathlib import Path
from threading import Thread
import unittest

from gateway_gui import GatewayClient
from soem_gateway import GatewayConfig, GatewayHandler, SoemGateway


def _start_gateway():
    config = GatewayConfig(
        interface="en7",
        host="127.0.0.1",
        port=0,
        simple_ng=Path("/nonexistent/simple_ng"),
        slaveinfo=Path("/nonexistent/slaveinfo"),
        pdo_server=Path("/nonexistent/soem_pdo_server"),
        log_lines=50,
        start_with_sudo=False,
        inventory_with_sudo=False,
    )
    gateway = SoemGateway(config)
    handler = type("ConfiguredGatewayHandler", (GatewayHandler,), {"gateway": gateway})
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    host, port = server.server_address
    return server, thread, f"http://{host}:{port}"


class GatewayClientTests(unittest.TestCase):
    def test_status_and_lifecycle_calls_return_dicts(self) -> None:
        server, thread, base_url = _start_gateway()
        try:
            client = GatewayClient(base_url)

            status = client.master_status()
            self.assertTrue(status["ok"])
            # Not started yet -> STOPPED, and the field the GUI reads is present.
            self.assertEqual(status["master_status"], "STOPPED")

            self.assertTrue(client.safe_off()["ok"])
            # pause is accepted by the API even though the CLI backend is a stub.
            self.assertTrue(client.master_pause()["ok"])
            # identity is used as a connection check.
            self.assertIn("gateway", client.identity())
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)

    def test_unavailable_gateway_returns_structured_error(self) -> None:
        client = GatewayClient("http://127.0.0.1:1", timeout_s=1)
        result = client.master_status()
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"], "gateway_unavailable")


if __name__ == "__main__":
    unittest.main()
