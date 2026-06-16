# Platform Strategy

One gateway, two platforms. The HTTP API and gateway code are identical on Linux
and macOS; only the native SOEM binaries differ.

## Why a Darwin backend

The upstream SOEM NIC backend uses Linux `PF_PACKET` raw sockets, which macOS
does not provide. The Darwin backend uses BPF descriptors instead, so the SOEM
samples (`slaveinfo`, `simple_ng`, …) build and run on macOS. See
[Ports](../ports/index.md) and the
[BPF runtime model](../ports/bpf_runtime_model.md).

## One gateway, per-platform binaries

`soem_gateway.py` does not care which NIC backend is underneath. It locates the
build output for the current platform and supervises it. The default binary
search order is:

```text
build/darwin-arm64/bin/<name>
build/darwin-x86_64/bin/<name>
build/default/bin/<name>
build-darwin/samples/<name>/<name>
build-darwin-x86_64/samples/<name>/<name>
```

You can always override with `--slaveinfo` and `--simple-ng`.

This means the same gateway can run:

- on a Linux host/teststand with the Linux raw-socket build;
- on a Raspberry-Pi-class node with the Linux build;
- on a developer Mac (`darwin-arm64` on Apple Silicon, `darwin-x86_64` on Intel)
  with the BPF build against a physical `en*` interface.

An automation host that points at the gateway sees the same JSON contract on all
of them; the platform difference is invisible above the process boundary.

## Build presets

| Preset | Target |
| --- | --- |
| `darwin-arm64` | Apple Silicon (M1–M-series) macOS build |
| `darwin-x86_64` | Intel macOS build (e.g. Mac mini 2014) |
| `default` | host default (Linux raw socket on Linux) |
| `docs` | upstream Sphinx docs target (external) |

See [Ports → Darwin arm64](../ports/darwin_arm64.md) and
[Ports → Darwin x86_64](../ports/darwin_x86_64.md) for the exact build commands.
