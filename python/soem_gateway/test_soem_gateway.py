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
import tempfile
from threading import Thread
import unittest
from http.server import ThreadingHTTPServer

from soem_gateway import (
    GatewayConfig,
    GatewayHandler,
    SoemGateway,
    _slaveinfo_configured_ok,
    _slaveinfo_has_error,
)


# A standalone stand-in for the real soem_pdo_server C helper: same line protocol
# (one JSON line per command), with a write->input loopback so READ after WRITE
# observes the value, mirroring the EL2004->EL1014 bench loopback.
_FAKE_PDO_SERVER = '''#!/usr/bin/env python3
import json, sys
inputs = [0] * 64
outputs = [0] * 64
print(json.dumps({"ok": True, "state": "running", "slaves": 2, "obytes": 1, "ibytes": 1, "expected_wkc": 3}), flush=True)
for raw in sys.stdin:
    parts = raw.split()
    if not parts:
        continue
    cmd = parts[0]
    if cmd == "STATUS":
        print(json.dumps({"ok": True, "state": "running", "slaves": 2, "obytes": 1, "ibytes": 1, "expected_wkc": 3}), flush=True)
    elif cmd == "READ" and len(parts) == 4:
        path, byte, bit = parts[1], int(parts[2]), int(parts[3])
        val = bool(inputs[byte] & (1 << bit))
        print(json.dumps({"ok": True, "path": path, "value": val, "input_byte": inputs[byte], "byte": byte, "bit": bit}), flush=True)
    elif cmd == "WRITE" and len(parts) == 5:
        path, byte, bit, value = parts[1], int(parts[2]), int(parts[3]), parts[4] in ("1", "true", "True")
        if value:
            outputs[byte] |= (1 << bit); inputs[byte] |= (1 << bit)
        else:
            outputs[byte] &= ~(1 << bit); inputs[byte] &= ~(1 << bit)
        print(json.dumps({"ok": True, "path": path, "value": value, "output_byte": outputs[byte], "byte": byte, "bit": bit}), flush=True)
    elif cmd == "SAFE_OFF":
        for i in range(len(outputs)):
            outputs[i] = 0; inputs[i] = 0
        print(json.dumps({"ok": True, "safe_off": True}), flush=True)
    elif cmd == "STOP":
        print(json.dumps({"ok": True, "stopped": True}), flush=True)
        break
    else:
        print(json.dumps({"ok": False, "error": "unknown_command"}), flush=True)
'''


def _make_gateway() -> SoemGateway:
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


class ProcessDataTests(unittest.TestCase):
    """Exercise the real per-path process-data path via a fake soem_pdo_server."""

    def _gateway(self, tmp: str) -> SoemGateway:
        helper = Path(tmp) / "fake_pdo_server.py"
        helper.write_text(_FAKE_PDO_SERVER, encoding="utf-8")
        helper.chmod(0o755)
        config = GatewayConfig(
            interface="eth0",
            host="127.0.0.1",
            port=0,
            simple_ng=Path("/nonexistent/simple_ng"),
            slaveinfo=Path("/nonexistent/slaveinfo"),
            pdo_server=helper,
            log_lines=50,
            start_with_sudo=False,
            inventory_with_sudo=False,
        )
        gateway = SoemGateway(config)
        gateway.request_config(
            {
                "inventory": [{"slot": 2, "model": "EL2004"}, {"slot": 3, "model": "EL1014"}],
                "catalog": {
                    "parameters": [
                        {"path": "ethercat.slot_2.el2004.output", "access": "write", "value_type": "bool", "process_byte_offset": 0, "process_bit_offset": 0},
                        {"path": "ethercat.slot_3.el1014.input", "access": "read", "value_type": "bool", "process_byte_offset": 0, "process_bit_offset": 0},
                    ]
                },
            }
        )
        return gateway

    def test_start_write_read_loopback_safe_off_stop(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            gateway = self._gateway(tmp)

            # When the pdo server is available, the master is the real helper.
            self.assertEqual(gateway.identity()["process_data_backend"], "soem_pdo_server")
            started = gateway.start("eth0")
            self.assertEqual(started["backend"], "soem_pdo_server")
            self.assertEqual(started["slaves"], 2)
            self.assertEqual(gateway.state.status()["master_status"], "RUNNING")

            # Write the output high -> the fake loops it onto the input.
            written = gateway.write({"path": "ethercat.slot_2.el2004.output", "value": True})
            self.assertTrue(written["ok"])
            self.assertEqual(written["quality"], "process_image")
            self.assertEqual(written["output_byte"], 1)

            read = gateway.read("ethercat.slot_3.el1014.input")
            self.assertEqual(read["quality"], "process_image")
            self.assertTrue(read["value"])

            safe = gateway.safe_off()
            self.assertIn("pdo", safe)
            # After safe-off the looped input reads back low.
            self.assertFalse(gateway.read("ethercat.slot_3.el1014.input")["value"])

            stopped = gateway.stop()
            self.assertTrue(stopped["ok"])
            self.assertEqual(gateway.state.status()["master_status"], "STOPPED")


class InventoryErrorDetectionTests(unittest.TestCase):
    """A successful enumeration must not be flagged because of benign CoE probes."""

    def test_sdo_probe_note_is_not_an_inventory_error(self) -> None:
        out = (
            "Time:1781624623.404 SDO slave:4 index:1c13.01 error:06090011 Subindex does not exist\n"
            "6 slaves found and configured.\n"
            "Slave:1\n Name:EK1101\n"
        )
        self.assertTrue(_slaveinfo_configured_ok(out))
        self.assertFalse(_slaveinfo_has_error(out, ""))

    def test_real_failures_are_still_flagged(self) -> None:
        self.assertTrue(_slaveinfo_has_error("No slaves found", ""))
        self.assertTrue(_slaveinfo_has_error("No socket connection on eth0\nExcecute as root", ""))
        # A hard error during an otherwise-found run is still a failure.
        self.assertTrue(_slaveinfo_has_error("6 slaves found and configured.\nbus timeout", ""))


if __name__ == "__main__":
    unittest.main()
