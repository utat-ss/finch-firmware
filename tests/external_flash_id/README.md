# Dual external SEMPER identification test

Static review complete; compilation and hardware operation are PENDING on the
Raspberry Pi. No local build, Docker, dependency installation, SSH connection,
or hardware operation was performed in this review.

## Scope and wiring

Target: `nucleo_h743zi`. SPI1 SCLK=PA5, MISO=PA6, MOSI=PA7; active-low
CS0=PE4 and CS1=PE5 with pull-ups. Use MCU pin names and the schematic for
your exact board revision; copied H753 connector-number claims were removed.
The upstream H743 default MOSI is PB5; this overlay deliberately selects PA7.
Ethernet MAC uses PA7 for RMII_CRS_DV, so MAC and MDIO are disabled here.
Disabling software does not disconnect an onboard PHY electrically: verify
PA7's schematic connection/jumper arrangement before wiring the flash board.
Console is USART3 PD8/PD9, 115200 baud, normally routed to ST-LINK VCP.

SPI is master, mode 0, 8-bit, MSB-first, single-line full duplex. Requested
maximum clock is 1,875,000 Hz; the STM32 divider may select a lower rate.
CS setup/hold delays are each requested as 1 microsecond. Both chip selects
are made inactive (physical HIGH) before the first transfer, followed by a
100 ms settling delay. Tests run separately for each chip.

Only four commands can pass the transaction helper:

| Opcode | Operation | Response bytes | Total TX/RX bytes |
| --- | --- | --- | --- |
| 9F | Read device ID | 6 | 7 |
| 05 | Read status register 1 | 1 | 2 |
| 07 | Read status register 2 | 1 | 2 |
| 35 | Read volatile configuration register 1 | 1 | 2 |

Every command uses one synchronous `spi_transceive_dt` call with equal TX/RX
lengths. TX is command plus zero fill; response starts at RX byte 1. Buffers
are seven bytes, with index, pointer, length, and opcode guards. There are no
write-enable, unlock, reset, program, erase, format, configuration-write,
error-clear, suspend/resume, or mode-changing commands. Flash/filesystem
Kconfig options are disabled, with build assertions to prevent accidentally
linking those features or Omar's flash/littlefs libraries into this test.

The test requires 1-1-1 SPI and factory zero register latency (VRGLAT=00).
It does not read CFR3 or change latency. Accepted IDs are Infineon 34,
HL-T/HS-T 2A/2B, density 1A/1B (512 Mbit/1 Gbit), ID length 0F, family 90.
Architecture byte 4 is recorded, not asserted. SR1 busy, WEL, erase-error,
or program-error bits cause a test failure after logging; no flags are cleared.
This checks supported family and clean idle state, not an exact board BOM.

## Static verification and references

Reviewed every file in this directory: `src/main.c`, `CMakeLists.txt`,
`prj.conf`, `testcase.yaml`, `boards/nucleo_h743zi.overlay`, this README,
and the new `dts/bindings/spi/zephyr,spi-device.yaml`.

Checked C declarations/control flow, API signatures, GPIO logical polarity,
SPI buffer lengths, ZTEST/ZTEST_SUITE structure, CMake initialization order,
Kconfig definitions, board metadata, DTS property syntax and bindings, pinctrl
labels, CS mapping, Ethernet conflict, and the complete SPI command path.
These are source-level checks, not compiler, CMake, Kconfig, dtc, or Twister runs.

Zephyr sources were inspected at the manifest revision
`3568e1b6d5cdd51a6b964a2a1d6d29200fea2056` (v4.3.0); STM32 pinctrl at its
pinned HAL revision `286dd285b5bb4fddafdfff27b5405264e5a61bfe`.
The Zephyr tree has no binding for `zephyr,spi-device`, so the test now supplies
a local binding inheriting `spi-device.yaml`, including its `on-bus: spi`
and default properties. CMake adds this application to DTS_ROOT before Zephyr.
No flash device driver is instantiated for these driverless child nodes.

- [Pinned H743 board DTS](https://github.com/zephyrproject-rtos/zephyr/blob/3568e1b6d5cdd51a6b964a2a1d6d29200fea2056/boards/st/nucleo_h743zi/nucleo_h743zi.dts)
- [Pinned SPI API](https://github.com/zephyrproject-rtos/zephyr/blob/3568e1b6d5cdd51a6b964a2a1d6d29200fea2056/include/zephyr/drivers/spi.h)
- [Infineon SEMPER datasheet 002-12345 Rev. AD](https://community.infineon.com/gfawx74859/attachments/gfawx74859/twnor-flash/1203/1/flash.pdf): sections 2.2.1, 4.7; Tables 51, 73, 89.

The attachment mentioned in the request was not accessible. Commands and ID
fields were checked against this manufacturer copy; comparison against the
actual attachment remains pending.

## Raspberry Pi build (later)

In the existing activated west/Zephyr/compiler environment, from the repository
root containing this test, run exactly:

```bash
west build -p always -b nucleo_h743zi -d build/external_flash_id tests/external_flash_id
```

Do not install/update anything for this step. The Pi workspace should match the
manifest revisions above. After building, record the complete build output and
inspect `build/external_flash_id/zephyr/.config` and `zephyr.dts` to confirm the
feature exclusions, local binding, SPI1 routing, and PE4/PE5 mapping. Expected
ELF path is `build/external_flash_id/zephyr/zephyr.elf`; it has not been produced
or verified by this review.

## Later flash/run (only after a successful Pi build)

Assumes the Nucleo ST-LINK USB probe is connected directly to the Pi and pyOCD
is already available there. Run these from the same repository root, without
Docker or a remote GDB server. List serial ports in one Pi terminal:

```bash
python -m serial.tools.list_ports
python -m serial.tools.miniterm /dev/ttyACM0 115200
```

`/dev/ttyACM0` is an example: substitute the ST-LINK VCP shown by the listing.
Use 115200, 8N1, no flow control. Keep capture open, then in a second Pi terminal:

```bash
west flash -d build/external_flash_id -r pyocd -- --tool-opt="--pack=$PWD/blobs/Keil.STM32H7xx_DFP.4.1.2.pack"
```

The pinned board runner supplies target `stm32h743zitx`; `--tool-opt` passes the
repository's existing CMSIS pack to pyOCD. This later flash command programs the
MCU's internal flash. It does not address the external SEMPER chips. Do not add
`--erase`. After flashing, press the Nucleo RESET button to start/repeat the test
and capture the full output. No test commands need to be sent over serial.
If pyOCD, probe access, pack support, or serial permissions are unavailable,
that is a later host prerequisite; it has not been verified or installed here.

## Expected serial output (not observed)

Zephyr banner/ztest formatting and test order may vary. With HL-T 512 Mbit
devices in their factory configuration, application lines should resemble:

```text
External flash read-only test: SPI1, PE4/PE5, mode 0, requested 1875000 Hz
Requires 1-1-1 SPI and factory zero register latency; no settings changed
Flash 0 (PE4) ID: 34 2A 1A 0F 03 90
Flash 0 SR1=SS SR2=TT CFR1V=CC (raw, zero latency)
Flash 0 BUSY=0 WEL=0; status/configuration recorded, not modified
Flash 1 (PE5) ID: 34 2A 1A 0F 03 90
Flash 1 SR1=SS SR2=TT CFR1V=CC (raw, zero latency)
Flash 1 BUSY=0 WEL=0; status/configuration recorded, not modified
```

SS/TT/CC are measured hexadecimal bytes, not predicted values. HS-T uses 2B;
1 Gbit uses 1B. Both identification tests should PASS and ztest should report
success. Transaction/ID failures can prevent register output for that chip.
An abnormal SR1 causes failure without recovery or modification.

## Record and unresolved risks

Record full build and serial logs, firmware commit, board revision, probe/port,
wiring/supply voltage, physical chip markings, all six ID bytes and three raw
registers per CS, assertion text, and reset/cold-power-cycle repeatability.

Compilation, generated DTS/Kconfig, warning cleanliness, linked drivers, Pi's
actual Zephyr version, runner availability, and firmware startup remain pending.
Hardware must confirm supply/level compatibility, PA7's PHY connection,
CS isolation, clocks, protocol/latency, register values, and chip identity.
Matching IDs alone cannot prove that two independent chips responded. Nondefault
latency, QPI, SafeBoot, AutoBoot, or deep-power-down state may invalidate reads.
The test performs no mode recovery. GPIO CS edges can affect power state even
though none of these commands writes array/configuration data.

This test does not establish full protection state, address mode, sector map,
Endurance Flex layout, page/ECC constraints, data integrity, write/erase
functionality, or littlefs suitability. CFR2/CFR3/SFDP discovery is future work.
No writable driver, filesystem integration, partitions, or MCUboot changes
are included; the upstream board's existing internal layout is inherited.
