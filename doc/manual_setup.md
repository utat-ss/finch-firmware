## Manual Setup (Ubuntu/WSL)

This guide is for **Ubuntu 24.04 LTS**. It also works on Ubuntu 24.04 running in WSL.

#### 1. Update the System:

```sh
sudo apt update
sudo apt upgrade
```

#### 2. Make an Empty Directory

```sh
mkdir ~/finch
cd ~/finch
```

**Note:** This is because the west manifest will populate one directory above this repository.

#### 3. Clone This Repository

Using **HTTPS**:

```sh
git clone https://github.com/utat-ss/finch-firmware.git
cd finch-firmware
```

Or using **SSH**:

```sh
git clone git@github.com:utat-ss/finch-firmware.git
cd finch-firmware
```

#### 4. Install Dependencies

```sh
sudo ./scripts/install_dependencies.sh
```

#### 5. Set Up Python Virtual Environment

```sh
./scripts/setup_python_venv.sh
```

#### 6. Set Up West Workspace

```sh
./scripts/setup_west_workspace.sh
```

On this step the Zephyr SDK is installed, so it might take a while.

#### 7. Install `pyocd` (for flashing onto the dev boards)

```sh
pip install pyocd
pyocd pack install stm32g431rbtx stm32h753zitx
```

### Steps to do before using the manual environment

Every time you open a terminal to build the apps, you need to activate the virtual environment and set the Finch Flight Software environment variable.

#### 1. Set Zephyr Environment Variables

```sh
source ../zephyr/zephyr-env.sh
```

#### 2. Activate Python Virtual Environment

```sh
source ../.venv/bin/activate
```

### Flashing from the Manual Environment

We suppose that the board is available through USB.

To flash the board, run the `west flash` command. To select a different runner, use the `-r` argument. `pyocd` is the recommended runner.

## Manual Setup (Windows)

This guide is for **Windows**.

#### 1. Make an Empty Directory

Open PowerShell in administrator mode. Then, `cd` to a folder you desire, and:

```sh
mkdir finch
cd finch
```

#### 2. Clone the finch-firmware Respository:

```sh
git clone https://github.com/utat-ss/finch-firmware.git
```

#### 3. Setup Python venv:

```sh
python -m venv .venv
./.venv/Scripts/activate
```

#### 4. Download Zephyr SDK

Download [Zephyr SDK](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.17.4). In particular, download the Minimal SDK Bundle for Windows, unzip it and put it in the `finch` directory. Name the folder `zephyr-sdk`.

Then, download the `arm-zephyr-eabi` toolchain for Windows x86-64. Unzip it and place its contents within the `zephyr-sdk`folder. The resulting folder structure should look like this:

```
finch/
├── zephyr-sdk/
│   ├── arm-zephyr-eabi/
│   ├── cmake/
│   ├── sdk_toolchains
│   ├── sdk_version
│   ├── setup.sh
│   └── zephyr-sdk-0.17.4/
└── finch-firmware/
```

#### 5. Setup West Workspace

```sh
$env:FINCH_FIRMWARE_ROOT = (Get-Location).Path
pip install west
cd finch-firmware

west init --local --mf west.yml
west update
west zephyr-export
west packages pip --install
west sdk install --install-dir "$(Split-Path $env:FINCH_FIRMWARE_ROOT -Parent)\zephyr-sdk" --toolchains arm-zephyr-eabi
```

#### 6. Install `pyocd` (for flashing onto the dev boards)

```sh
pip install pyocd
pyocd pack install stm32g431rbtx stm32h753zitx
```
