# MAGIC's embedded (hardware) solution

An example of MAGIC protocol transmitted over Bluetooth using COTS hardware.

A Docker image is available to build the software project binary.

The project utilizes [Zephyr OS](https://docs.zephyrproject.org/latest/index.html). Zephyr OS is an open source kernel designed for embedded systems. The kernel supports numerous hardware platforms. Changing the `-b` option when executing  `west build` will compile for different [hardware platforms supported by Zephyr](https://docs.zephyrproject.org/latest/boards/index.html).

This example is utilizing the [NRF52840 USB dongle](https://www.nordicsemi.com/Products/Development-hardware/nRF52840-Dongle).

## Create Docker container
```
# docker build -t nrfconnect-sdk .
docker build --platform linux/amd64 -t nrfconnect-sdk .
```
> **&#9432; Note** The container build will take a significant amount of time to complete

## Build software in container
For a pristine build (clean build but longer build time) run the following command. 
```
docker run --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project nrfconnect-sdk west build -p always -b nrf52840dongle_nrf52840
```

## Note: $pwd is a Linux command that prints the current directory path from root. If you're on MacOS or Windows, substitute that path in instead.

For faster builds remove the pristine option `-p always`.
```
docker run --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project nrfconnect-sdk west build -b nrf52840dongle_nrf52840
```

## Flash device with built software
Plug in dongle and run west flash from container. 
```
docker run --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project nrfconnect-sdk west flash
```

> **&#9432; Note**   
> Windows and Apple silicon Macs  
> 
> Docker containers will not have access to USB devices on Windows and Apple silicon Macs. For these platforms install the Desktop nRF Connect app for flash programming. The nRF Connect app can be used to flash the built image to the device as well as other various testing tools. Download and install [nRF Connect from Nordic](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop/Download?lang=en#infotabs) for your platform and follow steps below for flashing using nRF Connect app.
> 1. Open nRF Connect app. 
> 2. From the list of apps find the programmer app.
> 3. If not previously installed, click install to install the programmer app. 
> 4. Open the programmer app. 
> 5. Connect the target device and verify it is visible in the programmer select device list.
> 6. Plug in the Nordic Dongle
> 7. Click select device. Click on the Nordic device in the list (if device does not appear try resetting device by pressing reset button).
> 8. Add the built hex file to be flashed
    * Under `FILE` click `Add file` 
    * Select file `./project/build/zephyr/zephyr.hex`
> 9. Flash the device
    * Under `DEVICE` click the `Write` button
> 10. When write completes device should reboot and run the flashed firmware.

## Run interactive Docker container
To run the container in interactive mode. 
```
docker run -it --rm --mount type=bind,source=$pwd/project,target=/workdir/project -w /workdir/project nrfconnect-sdk /bin/bash
```
