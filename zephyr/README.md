# West patches in this repository

See [doc/west-patch.md](../doc/west-patch.md) for the directory convention,
required `patches.yml` fields, checksum algorithm, and commands.

This is the manifest repository location that `west patch` reads by default:
`patches.yml` plus `patches/`. The `zephyr/` directory here is unrelated to the
Zephyr source checkout at the workspace root.

```sh
./scripts/container.sh patch
./scripts/container.sh patch-list
./scripts/container.sh build
python3 scripts/generate-patch-manifest.py
```

| Path | Purpose |
|---|---|
| `usp_zephyr/0001-zephyr-4.4-warning-fixes.patch` | Zephyr 4.4 API and warning fixes |
| `usp_zephyr/0002-fix-lr-fhss-src-path.patch` | Correct LR-FHSS source path |
| `usp_zephyr/0003-xiao-nrf54l15-full-name.patch` | Add the required Xiao board `full_name` |
| `usp_zephyr/0004-sx1262-pa-compile-definitions.patch` | Propagate the SX1262 model macro to the BSP |
