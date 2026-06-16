# Install

The gateway is a single standard-library Python script. Its only runtime
dependency is a built set of SOEM sample binaries (`slaveinfo`, `simple_ng`) for
the current platform.

## Requirements

- Python 3.10+ (standard library only — no third-party packages).
- Built SOEM samples for the platform (see [Ports](../ports/index.md)).
  - Linux: the upstream raw-socket build.
  - macOS: the `darwin-arm64` or `darwin-x86_64` build.

## Build the SOEM samples

Linux (host default):

```sh
cmake --preset default
cmake --build --preset default
```

macOS (Apple Silicon):

```sh
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64
```

macOS (Intel):

```sh
cmake --preset darwin-x86_64
cmake --build --preset darwin-x86_64
```

The gateway auto-discovers the resulting binaries (see
[Binary discovery](#binary-discovery)); you can also point at them explicitly with
`--slaveinfo` and `--simple-ng`.

## Binary discovery

When `--slaveinfo`/`--simple-ng` are not given, the gateway searches, in order:

```text
build/darwin-arm64/bin/<name>
build/darwin-x86_64/bin/<name>
build/default/bin/<name>
build-darwin/samples/<name>/<name>
build-darwin-x86_64/samples/<name>/<name>
```

If none exist it falls back to the first candidate path, and inventory/start
requests will report the binary as missing until it is built or overridden.

## Smoke check

```sh
python3 python/soem_gateway/soem_gateway.py --interface en6 --port 8765 &
curl -s http://127.0.0.1:8765/health
```

`/health` works without root and without EtherCAT traffic. For a real inventory
you need a built `slaveinfo`, a connected segment and root — see
[Usage](usage.md).

## Tests

The gateway has standard-library `unittest` tests next to it:

```sh
python3 python/soem_gateway/test_soem_gateway.py
python3 python/soem_gateway/test_gateway_gui.py
```

These cover the SDO raw-transport round trip and the GUI's HTTP client. They run
without hardware and without a display.
