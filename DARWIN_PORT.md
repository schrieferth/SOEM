# Darwin/macOS Port Notes

This branch adds an experimental Darwin/macOS backend for SOEM (BPF-based NIC
access) with `darwin-arm64` and `darwin-x86_64` build presets, plus an
external-process HTTP gateway.

The detailed documentation now lives in the structured docs tree:

- **Ports overview:** [docs/ports/index.md](docs/ports/index.md)
- **Apple Silicon build:** [docs/ports/darwin_arm64.md](docs/ports/darwin_arm64.md)
- **Intel build:** [docs/ports/darwin_x86_64.md](docs/ports/darwin_x86_64.md)
- **BPF runtime model, self-frame filter, permissions, verified hardware:**
  [docs/ports/bpf_runtime_model.md](docs/ports/bpf_runtime_model.md)
- **HTTP gateway (install, usage, API):** [docs/gateway/index.md](docs/gateway/index.md)
- **Decision record:** [docs/adr/ADR-001-darwin-bpf-backend.md](docs/adr/ADR-001-darwin-bpf-backend.md)

Quick start (Apple Silicon):

```sh
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64
sudo build/darwin-arm64/bin/slaveinfo en7
```

Quick start (Intel):

```sh
cmake --preset darwin-x86_64
cmake --build --preset darwin-x86_64
```

Build the documentation with `cd docs && make html`.
