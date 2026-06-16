#!/usr/bin/env python3
"""Tests for the SOEM Python gateway SDO mailbox surface.

The SOEM repository has no pytest configuration, so these tests use the standard
library `unittest` and can run directly with `python3 test_soem_gateway.py` or be
discovered by pytest. They exercise the gateway both directly and over HTTP,
mirroring the framework EtherCAT gateway contract so the SOEM gateway and the
Signalzwerg gateway stay API-compatible.
"""

from __future__ import annotations

from http.client import HTTPConnection
import json
from pathlib import Path
from threading import Thread
import unittest
from http.server import ThreadingHTTPServer

from soem_gateway import GatewayConfig, GatewayHandler, SoemGateway


def _make_gateway() -> SoemGateway:
    config = GatewayConfig(
        interface="en7",
        host="127.0.0.1",
        port=0,
        simple_ng=Path("/nonexistent/simple_ng"),
        slaveinfo=Path("/nonexistent/slaveinfo"),
        log_lines=50,
        start_with_sudo=False,
        inventory_with_sudo=False,
    )
    gateway = SoemGateway(config)
    gateway.request_config(
        {
            "inventory": [{"slot": 2, "model": "EL2004", "vendor_id": "0x00000002"}],
            "catalog": {"parameters": []},
        }
    )
    return gateway


class SdoTransportTests(unittest.TestCase):
    def test_raw_write_then_read_round_trips(self) -> None:
        gateway = _make_gateway()
        # The gateway is a dumb raw mailbox: it stores and returns raw values and
        # reports the canonical hex address. #x and 0x notation map to the same
        # address. No names, types or access policy are added by the gateway.
        write = gateway.sdo_write(2, "0x8000", "0x01", 9)
        self.assertTrue(write["ok"])
        self.assertEqual(write["index"], "0x8000")
        self.assertEqual(gateway.sdo_read(2, "#x8000", 1)["value"], 9)

    def test_unseeded_object_reads_back_null(self) -> None:
        gateway = _make_gateway()
        read = gateway.sdo_read(2, 0x1018, 1, "uint")
        self.assertTrue(read["ok"])
        self.assertIsNone(read["value"])
        self.assertEqual(read["index"], "0x1018")


class SdoHttpTests(unittest.TestCase):
    def test_http_sdo_read_and_write_round_trip(self) -> None:
        gateway = _make_gateway()
        handler = type("ConfiguredGatewayHandler", (GatewayHandler,), {"gateway": gateway})
        server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        host, port = server.server_address
        try:
            conn = HTTPConnection(host, port, timeout=2)

            conn.request(
                "POST",
                "/ethercat/slave/2/sdo",
                body=json.dumps({"index": "0x8000", "subindex": "0x01", "value": 4}),
                headers={"Content-Type": "application/json"},
            )
            written = json.loads(conn.getresponse().read().decode("utf-8"))
            self.assertTrue(written["ok"])
            self.assertEqual(written["value"], 4)

            conn.request("GET", "/ethercat/slave/2/sdo?index=0x8000&subindex=0x01")
            read = json.loads(conn.getresponse().read().decode("utf-8"))
            self.assertTrue(read["ok"])
            self.assertEqual(read["value"], 4)
            self.assertEqual(read["index"], "0x8000")
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
