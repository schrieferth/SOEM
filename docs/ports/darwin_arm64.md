# Darwin arm64 (Apple Silicon)

The `darwin-arm64` preset builds SOEM for Apple Silicon Macs.

## Apple Silicon coverage

Apple Silicon Macs from M1 through later M-series generations use the same
`arm64` architecture target from SOEM's perspective. The Darwin backend does not
use CPU-generation-specific instructions, so the same `darwin-arm64` preset is
expected to cover M1, M2, M3, M4 and M5 machines, subject to normal macOS BPF
permissions.

## Build and install

```sh
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64
cmake --install build/darwin-arm64
```

Binaries are produced under `build/darwin-arm64/bin/` (and an install tree under
`build/darwin-arm64/install/bin/`). The gateway discovers
`build/darwin-arm64/bin/<name>` automatically.

## Verify

```sh
# Adapter discovery (no traffic, prints en0/en5/en6/...):
build/darwin-arm64/bin/slaveinfo

# Inventory on the EtherCAT interface (needs root):
sudo build/darwin-arm64/bin/slaveinfo en7
```

A supervised hardware run on `en7` found and configured a six-slave chain:
EK1101, EL2004, EL1014, EL4031, EL3104 and EL3681.

See the [BPF runtime model](bpf_runtime_model.md) for permissions and the
self-frame filter, and [Gateway → Usage](../gateway/usage.md) to drive it over
HTTP.
