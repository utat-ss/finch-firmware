# STM32 Dev Board Development Guide 

This document provides instructions for building/flashing Zephyr projects onto STM32 dev boards. 

>> **On Windows, this guide uses Powershell as a host terminal. Please don't run the commands in WSL or Git Bash or cmd.**

### 1. Setup
Ensure you are in the root-directory of your `finch-firmware` folder. Then, activate the Python venv `.venv`:
<details><summary>Windows</summary>

```powershell
.\.venv\Scripts\activate
```

</details>

<details><summary>macOS/Linux/WSL</summary>

```sh
source .venv/bin/activate
```

</details>
Then, ensure you set the necessary environmental variables:

<details><summary>Windows</summary>

```powershell
cd finch-firmware
$env:FINCH_FIRMWARE_ROOT = (Get-Location).Path

cd ../zephyr
$env:ZEPHYR_BASE = (Get-Location).Path

cd ../zephyr-sdk
$env:ZEPHYR_SDK_INSTALL_DIR = (Get-Location).Path

$env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
```

</details>

<details><summary>macOS/Linux/WSL</summary>

```sh
cd finch-firmware
export FINCH_FIRMWARE_ROOT="$(pwd)"

cd ../zephyr
export ZEPHYR_BASE="$(pwd)"

cd ../zephyr-sdk
export ZEPHYR_SDK_INSTALL_DIR="$(pwd)"

export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
```

</details>


### 2. Build

You can then build the project by running `west build`.

Example: `west build -p always -b nucleo_h753zi apps/pay` will build the `pay` app for our H7 dev board.

The build artifact is in the `/build` directory of the `finch-firmware` folder.

### 3. Flash

Ensure that the dev board is physically connected to your laptop via a micro-USB cable. Change directory into `finch-firmware`:
```sh
cd finch-firmware
```
Then, run:
```sh
pyocd flash --target stm32g431rbtx build/zephyr/zephyr.hex
```
where `stm32g431rbtx` references the STM32G431RB board. If you are using the STM32H753ZI board, use stm32h753zitx

### 5. View the logs

<details><summary>Windows</summary>

You can use a tool like puTTY/Tera Term/Realterm or others. The most commonly used baud rate is 115200, which is often not the baud rate set in those tools. Make sure to set the proper rate in the tool.

</details>

<details><summary>macOS</summary>

You can use minicom to view the logs. You can install minicom with:
```sh
brew install minicom
```

</details>

<details><summary>Linux</summary>

You can use minicom to view the logs. On Ubuntu, minicom can be installed with:
```sh
sudo apt-get install minicom
```

On Fedora:
```sh
sudo dnf install minicom
```

</details>
