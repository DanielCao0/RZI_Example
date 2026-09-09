# RZI Example

This repository is a customer-side Zephyr application and the integration
example for RZI. It consumes RZI as an independent west project and uses only
the public `<rzi/lorawan.h>` API.

## Workspace topology

```text
rzi-workspace/
├── .west/
├── app/                 this manifest repository
├── rzi/                 independent RZI SDK repository
├── zephyr/
├── usp_zephyr/
├── modules/
├── bootloader/
└── tools/
```

The dependency relationship is declared in [`west.yml`](west.yml). Application
CMake does not add RZI source directories or private include paths.

## Build and flash

Edit OTAA credentials in `boards/rzi_rak4631_nrf52840.overlay`, then run:

```sh
mkdir rzi-workspace
cd rzi-workspace
git clone https://github.com/DanielCao0/RZI_Example app
cd app
./scripts/container.sh build-image
./scripts/container.sh init
./scripts/container.sh build
./scripts/flash-rak4631.sh
```

The build output is written to the workspace-level `build/app` directory.

## Ownership boundary

The application owns product behavior, credentials, region selection, LEDs, and
the uplink schedule. Target the RZI product board `rzi_rak4631/nrf52840` with
sysbuild; flash `merged.hex`. See `rzi/doc/boot.md`.
