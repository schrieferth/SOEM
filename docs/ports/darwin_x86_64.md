# Darwin x86_64 (Intel)

The `darwin-x86_64` preset builds SOEM for Intel Macs, for example a Mac mini
2014 target.

## Build

```sh
cmake --preset darwin-x86_64
cmake --build --preset darwin-x86_64
```

Binaries are produced under `build/darwin-x86_64/bin/` (or
`build-darwin-x86_64/samples/<name>/<name>` depending on the build layout). The
gateway discovers both locations automatically; otherwise pass `--slaveinfo` /
`--simple-ng` explicitly.

## Verify

```sh
# Adapter discovery (no traffic):
build/darwin-x86_64/bin/slaveinfo

# Inventory on the EtherCAT interface (needs root):
sudo build/darwin-x86_64/bin/slaveinfo <ethercat-interface>
```

Use the physical Ethernet interface connected to the EtherCAT segment. On the Mac
mini 2014 this is most likely an `en*` interface reported by the `slaveinfo`
adapter list.

## Same runtime model as arm64

The Intel build uses the same BPF backend, self-frame filter and permission
boundary as the [arm64 build](darwin_arm64.md). Only the CPU target differs. See
the [BPF runtime model](bpf_runtime_model.md).
