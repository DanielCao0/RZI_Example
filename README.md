# RZI Example

This repository is a customer-side Zephyr application and the integration
example for RZI. It consumes RZI as an independent west project and uses the
public RZI initialization, LoRaWAN, and power APIs.

## Workspace topology

```text
rzi-workspace/
├── .west/
├── app/                 this manifest repository
├── rzi/                 independent RZI SDK repository
├── zephyr/
├── modules/
│   └── usp_zephyr/
├── bootloader/
└── tools/
```

The dependency relationship is declared in [`west.yml`](west.yml). Application
CMake does not add RZI source directories or private include paths.

## Board selection

This counter example has application configuration for the following targets:

| Board | `west build -b` target | Credentials and region | Console |
|---|---|---|---|
| RAK4631 (default) | `rak4631/nrf52840/rzi` | [`boards/rak4631_nrf52840_rzi.overlay`](boards/rak4631_nrf52840_rzi.overlay) | USB CDC ACM |
| Standard RAK3172 / RAK3372 | `rak3172/stm32wle5xx/rzi` | [`boards/rak3172_stm32wle5xx_rzi.overlay`](boards/rak3172_stm32wle5xx_rzi.overlay) | LPUART1, TX PA2 / RX PA3, 115200 8N1 |
| RAK3372-F / RAK3172-F module | `rak3372_f/stm32wle5xx` | [`boards/rak3372_f_stm32wle5xx.overlay`](boards/rak3372_f_stm32wle5xx.overlay) | LPUART1, TX PA2 / RX PA3, 115200 8N1 |

Keep `/rzi` in the RAK4631 and standard RAK3172 targets; it selects the SDK's
radio and partition configuration. The standard RAK3172 target uses a crystal;
RAK3172-T requires a separate TCXO configuration. The F target includes its
TCXO, RF switch, and external flash settings.

Edit the overlay for the selected board before building. Matching `.conf`
files are loaded automatically. The container's `build` command without
arguments selects RAK4631; the native commands below select each board
explicitly. LEDs require the PA0/PA1 carrier wiring on the STM32 targets.

For RAK3362, use the SDK AT sample with `-b rak3362/nrf54l15/cpuapp`.
Its UART20 is reached through the base board's USB-UART converter. The
[SDK board guide](https://github.com/DanielCao0/RZI#supported-boards) covers
that target and the other supported boards.

## Build and flash

Edit OTAA credentials in `boards/rak4631_nrf52840_rzi.overlay`, then run:

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

For a native build of the same RAK4631 variant, run from the workspace root
with the Zephyr environment active:

```sh
west build --sysbuild -b rak4631/nrf52840/rzi -d build/app-rzi-variant app
```

This target uses `boards/rak4631_nrf52840_rzi.overlay` for credentials and the
USB console. The merged bootloader and application image is
`build/app-rzi-variant/merged_rak4631_nrf52840_rzi.hex`.

For the standard RAK3172 with a crystal (STM32WLE5CC), use:

```sh
west build --sysbuild -b rak3172/stm32wle5xx/rzi -d build/app-rak3172-rzi app
```

This target uses LPUART1 on PA2/PA3 at 115200 baud through an external
USB-UART adapter. Its board configuration disables USB CDC and supplies the
credentials overlay. The merged image is
`build/app-rak3172-rzi/merged_rak3172_stm32wle5xx_rzi.hex`.
The single-slot map has no FUOTA. RAK3172-T requires its own hardware
configuration; see the RZI board variant's README.

For RAK3172-F with TCXO and external flash, use:

```sh
west build --sysbuild -b rak3372_f/stm32wle5xx -d build/app-rak3372-f app
```

This uses the F-specific clock, pinmux, RF switch and dual-slot layout. Its
merged image is `build/app-rak3372-f/merged_rak3372_f_stm32wle5xx.hex`.

## Ownership boundary

The application owns product behavior, credentials, region selection, LEDs, and
the uplink schedule. Build the selected target with sysbuild and flash its
merged bootloader/application HEX. The default RAK4631 container build also
publishes `build/app/merged.hex`. See the SDK's `rzi/doc/boot.md` for the boot
contract. Workspace topology and the local build pipeline are in
[`doc/README.md`](doc/README.md).
